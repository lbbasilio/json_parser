#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "json_parser.h"

// Utilities
static uint8_t json_is_whitespace(const char c)
{
	const char whitespace[] = " \t\r\n";
	return strchr(whitespace, c) != NULL;
}

static uint8_t json_is_escapable(const char c)
{
	const char escapable[] = "\"\\/bfnrt";
	return strchr(escapable, c) != NULL;
}

static uint8_t json_is_hex_digit(const char c)
{
	const char hex_digits[] = "0123456789ABCDEFabcdef";
	return strchr(hex_digits, c) != NULL;
}

static uint8_t json_is_dec_digit(const char c)
{
	const char dec_digits[] = "0123456789";
	return strchr(dec_digits, c) != NULL;
}

static uint8_t json_is_pos_digit(const char c)
{
	const char pos_digits[] = "123456789";
	return strchr(pos_digits, c) != NULL;
}

static uint8_t json_is_num_delim(const char c)
{
	const char num_delim[] = " \t\r\n,]}";
	return strchr(num_delim, c) != NULL;
}

static int parse_string(char** ptr, Json_Node_Value* value)
{
	value->string_data = NULL;

	// S0: Expecting " character
	// S1: Checking for \ character or end of string
	// S2: Expecting escapable character
	// S3-S6: Expecting hexadecimal digits
	enum string_machine_state { S0, S1, S2, S3, S4, S5, S6 };
	enum string_machine_state state = S0;

	char* it;
	for (it = *ptr; *it != '\0'; ++it) {
		switch (state) {

			case S0:	if (*it == '\"') state = S1;
						else return JSON_ERROR_STRING;
						break;

			case S1:	if (*it == '\\') state = S2;
						else if (*it == '\"') goto STR_SUCCESS;
						else state = S1;
						break;

			case S2:	if (*it == 'u') state = S3;
						else if (json_is_escapable(*it)) state = S1;
						else return JSON_ERROR_STRING;
						break;

			case S3:	
			case S4:
			case S5:	if (json_is_hex_digit(*it)) state++;
						else return JSON_ERROR_STRING;
						break;

			case S6:	if (json_is_hex_digit(*it)) state = S1;
						else return JSON_ERROR_STRING;
						break;
		}
	}

STR_SUCCESS:
	size_t len = it - *ptr;
	value->string_data = malloc(len);
	memcpy(value->string_data, (*ptr) + 1, len - 1);
	value->string_data[len-1] = '\0';

	*ptr = it;
	return JSON_OK;
}

static int parse_bool(char** ptr, Json_Node_Value* value)
{
	value->bool_data = 0;

	// S0: Expecting t or f characters
	// S1: Expecting r character
	// S2: Expecting u character
	// S2: Expecting e character
	// S2: Expecting a character
	// S2: Expecting l character
	// S2: Expecting s character
	// S2: Expecting e character
	enum bool_machine_state { S0, S1, S2, S3, S4, S5, S6, S7 };
	enum bool_machine_state state = S0;

	char* it;
	for (it = *ptr; *it != '\0'; ++it) {
		switch (state) {

			case S0:	if (*it == 't') state = S1;
						else if (*it == 'f') state = S4;
						else return JSON_ERROR_BOOL;
						break;

			case S1:	if (*it == 'r') state = S2;
						else return JSON_ERROR_BOOL;
						break;
						
			case S2:	if (*it == 'u') state = S3;
						else return JSON_ERROR_BOOL;
						break;
						
			case S3:	if (*it == 'e') {
							value->bool_data = true;
							goto BOOL_SUCCESS;
						}
						else return JSON_ERROR_BOOL;
						break;

			case S4:	if (*it == 'a') state = S5;
						else return JSON_ERROR_BOOL;
						break;

			case S5:	if (*it == 'l') state = S6;
						else return JSON_ERROR_BOOL;
						break;

			case S6:	if (*it == 's') state = S7;
						else return JSON_ERROR_BOOL;
						break;

			case S7:	if (*it == 'e') {
							value->bool_data = false;
							goto BOOL_SUCCESS;
						}
						else return JSON_ERROR_BOOL;
						break;
		}
	}
	
BOOL_SUCCESS:
	*ptr = it++;
	return JSON_OK;
}

static int parse_null2(char** ptr)
{
	// S0: Expecting n character
	// S1: Expecting u character
	// S2: Expecting l character
	// S3: Expecting l character
	enum null_machine_state { S0, S1, S2, S3 };
	enum null_machine_state state = S0;

	char* it;
	for (it = *ptr; *it != '\0'; ++it) {
		switch (state) {
			case S0:	if (*it == 'n') state = S1;
						else return JSON_ERROR_NULL;
						break;
			case S1:	if (*it == 'u') state = S2;
						else return JSON_ERROR_NULL;
						break;
			case S2:	if (*it == 'l') state = S3;
						else return JSON_ERROR_NULL;
						break;
			case S3:	if (*it == 'l') goto NULL_SUCCESS;
						else return JSON_ERROR_NULL;
						break;
		}
	}

NULL_SUCCESS:
	*ptr = it;
	return JSON_OK;
}

static int parse_value2(char** ptr, enum Json_Node_Type* type, Json_Node_Value* value);
static int parse_object(char** ptr, Json_Node_Value* value);

static int parse_array(char** ptr, Json_Node_Value* array)
{
	array->child = malloc(sizeof(Json_Node));
	memset(array->child, 0, sizeof(Json_Node));
	array->child->type = JSON_ARRAY;

	// S0: Expecting [ character
	// S1: Expecting ] character or json value, ignores whitespaces
	// S2: Expecting , or ] characters, ignores whitespace
	// S3: Expecting json value, ignores whitespaces
	enum array_machine_state { S0, S1, S2, S3 };
	enum array_machine_state state = S0;

	Json_Node* last_child = NULL;

	int error;
	char* it;
	for (it = *ptr; *it != '\0'; ++it) {
		switch (state) {

			case S0:	if (*it == '[') state = S1;
						else goto ARR_ERR;
						break;

			case S1:	if (*it == ']') goto ARR_SUCCESS;
						else if (json_is_whitespace(*it)) state = S1;
						else {

							// Parse JSON value
							Json_Node_Value value;
							enum Json_Node_Type type;
							if ((error = parse_value2(&it, &type, &value))) 
								goto ARR_ERR;

							// Populate node
							Json_Node* child;
							if (type == JSON_ARRAY || type == JSON_OBJECT) {
								child = value.child;
							}
							else {
								child = malloc(sizeof(Json_Node));
								memset(child, 0, sizeof(Json_Node));
								child->value = value;
							}
							child->type = type;

							// Insert node into parent
							if (last_child) {
								last_child->next = child;
								child->prev = last_child;
							}
							else {
								// TODO: WTF?
								array->child->value.child = child;
							}
							last_child = child;
							state = S2;
						}
						break;

			case S2:	if (*it == ',') state = S3;
						else if (*it == ']') goto ARR_SUCCESS;
						else if (json_is_whitespace(*it)) state = S2;
						else goto ARR_ERR;
						break;

			case S3:	if (json_is_whitespace(*it)) state = S3;
						else {
							// Parse JSON value
							Json_Node_Value value;
							enum Json_Node_Type type;
							if ((error = parse_value2(&it, &type, &value))) 
								goto ARR_ERR;

							// Populate node
							Json_Node* child;
							if (type == JSON_ARRAY || type == JSON_OBJECT) {
								child = value.child;
							}
							else {
								child = malloc(sizeof(Json_Node));
								memset(child, 0, sizeof(Json_Node));
								child->value = value;
							}
							child->type = type;

							// Insert node into parent
							if (last_child) {
								last_child->next = child;
								child->prev = last_child;
							}
							else {
								// TODO: WTF?
								array->child->value.child = child;
							}
							last_child = child;
							state = S2;
						}
		}
	}

ARR_ERR:
	// TODO: WTF?
	Json_Node* p = array->child->value.child;
	while (p) {
		Json_Node* temp = p->next;
		free(p);
		p = temp;
	}
	free(array->child);
	return JSON_ERROR_ARRAY;

ARR_SUCCESS:
	*ptr = it;
	return JSON_OK;
}

static int parse_number(char** ptr, enum Json_Node_Type* type, Json_Node_Value* value)
{
	value->long_data = 0;
	*type = JSON_LONG;

	enum number_machine_state { S0, S1, S2, S3, S4, S5, S6, S7, S8, S9 };
	enum number_machine_state state = S0;

	char* it;
	for (it = *ptr; *it != '\0'; ++it) {
		switch (state) {

			case S0:	if (*it == '-') state = S1;
						else if (*it == '0') state = S2;
						else if (json_is_pos_digit(*it)) state = S3;
						else return JSON_ERROR_NUMBER;
						break;

			case S1:	if (*it == '0') state = S2;
						else if (json_is_pos_digit(*it)) state = S3;
						else return JSON_ERROR_NUMBER;
						break;

			case S2:	if (*it == '.') state = S5;
						else if (*it == 'e' || *it == 'E') state = S7;
						else if (json_is_num_delim(*it)) goto NUM_SUCCESS;
						else return JSON_ERROR_NUMBER;
						break;

			case S3:	if (*it == '.') state = S5;
						else if (*it == 'e' || *it == 'E') state = S7;
						else if (json_is_dec_digit(*it)) state = S4;
						else if (json_is_num_delim(*it)) goto NUM_SUCCESS;
						else return JSON_ERROR_NUMBER;
						break;

			case S4:	if (*it == '.') state = S5;
						else if (*it == 'e' || *it == 'E') state = S7;
						else if (json_is_dec_digit(*it)) state = S4;
						else if (json_is_num_delim(*it)) goto NUM_SUCCESS;
						else return JSON_ERROR_NUMBER;
						break;

			case S5:	*type = JSON_DOUBLE;
						if (json_is_dec_digit(*it)) state = S6;
						else return JSON_ERROR_NUMBER;
						break;
						
			case S6:	if (*it == 'e' || *it == 'E') state = S7;
						else if (json_is_dec_digit(*it)) state = S6;
						else if (json_is_num_delim(*it)) goto NUM_SUCCESS;
						else return JSON_ERROR_NUMBER;
						break;

			case S7:	*type = JSON_DOUBLE;
						if (*it == '-' || *it == '+') state = S8;
						else if (json_is_dec_digit(*it)) state = S9;
						else return JSON_ERROR_NUMBER;
						break;

			case S8:	if (json_is_dec_digit(*it)) state = S9;
						else return JSON_ERROR_NUMBER;
						break;

			case S9:	if (json_is_dec_digit(*it)) state = S9;
						else if (json_is_num_delim(*it)) goto NUM_SUCCESS;
						else return JSON_ERROR_NUMBER;
						break;
		}
	}

NUM_SUCCESS:;
	
	char buffer[0x20];
	size_t len = it - *ptr + 2;
	memcpy(buffer, *ptr, len - 1);
	buffer[len-1] = '\0';
	if (*type == JSON_LONG) 
		value->long_data = atoll(buffer);
	else 
		value->double_data = atof(buffer);

	*ptr = --it;
	return JSON_OK;
}

static int parse_value2(char** ptr, enum Json_Node_Type* type, Json_Node_Value* value)
{
	value->long_data = 0;

	if (**ptr == '\"') {
		*type = JSON_STRING;
		return parse_string(ptr, value);
	}

	if (**ptr == 't' || **ptr == 'f') {
		*type = JSON_BOOL;
		return parse_bool(ptr, value);
	}

	if (**ptr == 'n') {
		*type = JSON_NULL;
		return parse_null2(ptr);
	}

	if (**ptr == '[') {
		*type = JSON_ARRAY;
		return parse_array(ptr, value);
	}

	if (**ptr == '{') {
		*type = JSON_OBJECT;
		return parse_object(ptr, value);
	}

	if (**ptr == '-' || json_is_dec_digit(**ptr)) {
		return parse_number(ptr, type, value);
	}

	// Failed to parse value
	return JSON_ERROR_VALUE;
}

static int parse_object(char** ptr, Json_Node_Value* object)
{
	object->child = malloc(sizeof(Json_Node));
	memset(object->child, 0, sizeof(Json_Node));
	object->child->type = JSON_OBJECT;

	// S0: Expecting { character
	// S1: Expecting " character, ignores whitespace
	// S2: Expecting : character, ignores whitespace
	// S3: Expecting JSON value (recursion entrypoint)
	// S4: Expecting , or } character, ignores whitespace
	enum object_machine_state { S0, S1, S2, S3, S4 };
	enum object_machine_state state = S0;

	Json_Node* last_child = NULL;

	int error = 0;
	char* current_key;
	char* it;
	for (it = *ptr; *it != '\0'; ++it) {
		switch (state) {

			case S0:	if (*it == '{') state = S1;
						else goto OBJ_ERR;
						break;

			case S1:	if (*it == '}') goto OBJ_SUCCESS;
						else if (json_is_whitespace(*it)) state = S1;
						else if (*it == '\"') {
							Json_Node_Value value;
							if ((error = parse_string(&it, &value)))
								goto OBJ_ERR;
							current_key = value.string_data;
							state = S2;
						}
						break;

			case S2:	if (*it == ':') state = S3;
						else if (json_is_whitespace(*it)) state = S2;
						else {
							error = JSON_ERROR_OBJECT;
							goto OBJ_ERR;
						}
						break;

			case S3:	if (json_is_whitespace(*it)) state = S3;
						else {
							
							// Parse JSON value
							Json_Node_Value value;
							enum Json_Node_Type type;
							if ((error = parse_value2(&it, &type, &value))) 
								goto OBJ_ERR;

							// Populate node
							Json_Node* child;
							if (type == JSON_ARRAY || type == JSON_OBJECT) {
								child = value.child; 
							}
							else {
								child = malloc(sizeof(Json_Node));
								memset(child, 0, sizeof(Json_Node));
								child->value = value;
							}
							child->type = type;
							child->key = current_key;
							current_key = NULL;

							// Insert node into parent
							if (last_child) {
								last_child->next = child;
								child->prev = last_child;
							}
							else {
								// TODO: WTF?
								object->child->value.child = child;
							}
							last_child = child;
							state = S4;
						}
						break;

			case S4:	if (*it == ',') state = S1;
						else if (*it == '}') goto OBJ_SUCCESS;
						else if (json_is_whitespace(*it)) state = S4;
						else goto OBJ_ERR;
		}
	}

OBJ_SUCCESS:
	*ptr = it;
	return JSON_OK;

OBJ_ERR:
	// Do not cleanup individual nodes
	// Object cleanup from error will come from the root
	if (current_key)
		free(current_key);
	return error;
}

Json_Node* json_parse(char* json, int* error)
{
	*error = 0;
	while (*json != '{' && *json != '\0') json++;

	Json_Node_Value value;
	if ((*error = parse_object(&json, &value))) {
		json_delete(value.child);
		return NULL;
	}
	
	return value.child;
}

Json_Node* json_get(Json_Node* object, char* key)
{
	if (object == NULL || object->type != JSON_OBJECT)
		return NULL;

	Json_Node* p = object->value.child;
	while (p) {
		if (strcmp(p->key, key) == 0)
			return p;
		p = p->next;
	}

	return NULL;
}

void json_delete(Json_Node* object)
{
	if (!object)
		return;

	if (object->key)
		free(object->key);

	if (object->type == JSON_OBJECT || object->type == JSON_ARRAY)
		json_delete(object->value.child);

	if (object->next)
		json_delete(object->next);
	
	free(object);
}
