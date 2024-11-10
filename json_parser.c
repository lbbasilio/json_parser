#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "json_parser.h"

#define CUP_LIST_IMPLEMENTATION
#include "list.h"

#include "hashtable.h"

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

static char* parse_string(char* start, char** str, int64_t* size) 
{
	*str = NULL;
	*size = 0;
	enum string_machine_state { S0, S1, S2, S3, S4, S5, S6 };
	enum string_machine_state state = S0;
	char* it;
	for (it = start; *it != '\0'; ++it) {
		switch (state) {
			case S0:	if (*it == '\"') state = S1;
						else goto STR_END;
						break;
			case S1:	if (*it == '\\') state = S2;
						else if (*it == '\"') goto STR_SUCCESS;
						else state = S1;
						break;
			case S2:	if (*it == 'u') state = S3;
						else if (json_is_escapable(*it)) state = S1;
						else goto STR_END;
						break;
			case S3:	
			case S4:
			case S5:	if (json_is_hex_digit(*it)) state++;
						else goto STR_END;
						break;
			case S6:	if (json_is_hex_digit(*it)) state = S1;
						else goto STR_END;
						break;
		}
	}
	goto STR_END;

STR_SUCCESS:;
	int64_t len = it - start;
	*str = malloc(len);
	memcpy(*str, start + 1, len - 1);
	(*str)[len-1] = '\0';
	*size = len;

STR_END:
	return ++it;
}

static char* parse_number(char* start, void** num, int64_t* size)
{
	*num = NULL;
	*size = 0;
	enum number_machine_state { S0, S1, S2, S3, S4, S5, S6, S7, S8, S9 };
	enum number_machine_state state = S0;
	uint8_t is_float = 0;
	char* it;
	for (it = start; *it != '\0'; ++it) {
		switch (state) {
			case S0:	if (*it == '-') state = S1;
						else if (*it == '0') state = S2;
						else if (json_is_pos_digit(*it)) state = S3;
						else goto NUM_END;
						break;
			case S1:	if (*it == '0') state = S2;
						else if (json_is_pos_digit(*it)) state = S3;
						else goto NUM_END;
						break;
			case S2:	if (*it == '.') state = S5;
						else if (*it == 'e' || *it == 'E') state = S7;
						else if (json_is_num_delim(*it)) goto NUM_SUCCESS;
						else goto NUM_END;
						break;
			case S3:	if (*it == '.') state = S5;
						else if (*it == 'e' || *it == 'E') state = S7;
						else if (json_is_dec_digit(*it)) state = S4;
						else if (json_is_num_delim(*it)) goto NUM_SUCCESS;
						else goto NUM_END;
						break;
			case S4:	if (*it == '.') state = S5;
						else if (*it == 'e' || *it == 'E') state = S7;
						else if (json_is_dec_digit(*it)) state = S4;
						else if (json_is_num_delim(*it)) goto NUM_SUCCESS;
						else goto NUM_END;
						break;
			case S5:	is_float = 1;
						if (json_is_dec_digit(*it)) state = S6;
						else goto NUM_END;
						break;
			case S6:	if (*it == 'e' || *it == 'E') state = S7;
						else if (json_is_dec_digit(*it)) state = S6;
						else if (json_is_num_delim(*it)) goto NUM_SUCCESS;
						else goto NUM_END;
						break;
			case S7:	is_float = 1;
						if (*it == '-' || *it == '+') state = S8;
						else if (json_is_dec_digit(*it)) state = S9;
						else goto NUM_END;
						break;
			case S8:	if (json_is_dec_digit(*it)) state = S9;
						else goto NUM_END;
						break;
			case S9:	if (json_is_dec_digit(*it)) state = S9;
						else if (json_is_num_delim(*it)) goto NUM_SUCCESS;
						else goto NUM_END;
						break;
		}
	}

	goto NUM_END;

NUM_SUCCESS:;
	int64_t len = it - start + 2;
	char* s = malloc(len);
	memcpy(s, start, len - 1);
	s[len-1] = '\0';
	if (is_float) {
		*num = malloc(sizeof(double));
		*((double*)*num) = atof(s);
		*size = sizeof(double);
	} else {
		*num = malloc(sizeof(int64_t));
		*((int64_t*)*num) = atoll(s);
		*size = sizeof(int64_t);
	}

NUM_END:
	return it;
}

static char* parse_bool(char* start, int64_t** b, int64_t* size)
{
	*b = NULL;
	*size = 0;
	enum bool_machine_state { S0, S1, S2, S3, S4, S5, S6, S7 };
	enum bool_machine_state state = S0;
	uint8_t val = 0;
	char* it;
	for (it = start; *it != '\0'; ++it) {
		switch (state) {
			case S0:	if (*it == 't') state = S1;
						else if (*it == 'f') state = S4;
						else goto BOOL_END;
						break;
			case S1:	if (*it == 'r') state = S2;
						else goto BOOL_END;
						break;
			case S2:	if (*it == 'u') state = S3;
						else goto BOOL_END;
						break;
			case S3:	if (*it == 'e') {
							val = 1;
							goto BOOL_SUCCESS;
						}
						else goto BOOL_END;
						break;
			case S4:	if (*it == 'a') state = S5;
						else goto BOOL_END;
						break;
			case S5:	if (*it == 'l') state = S6;
						else goto BOOL_END;
						break;
			case S6:	if (*it == 's') state = S7;
						else goto BOOL_END;
						break;
			case S7:	if (*it == 'e') {
							val = 0;
							goto BOOL_SUCCESS;
						}
						else goto BOOL_END;
						break;
		}
	}
	
	goto BOOL_END;

BOOL_SUCCESS:
	*b = malloc(sizeof(int64_t));
	**b = (int64_t)val;
	*size = sizeof(int64_t);

BOOL_END:
	return ++it;
}

static char* parse_null(char* start, void** ptr, int64_t* size)
{
	*ptr = (void*)UINTPTR_MAX;	// Assign non null value
	*size = sizeof(void*);
	enum null_machine_state { S0, S1, S2, S3 };
	enum null_machine_state state = S0;
	char* it;
	for (it = start; *it != '\0'; ++it) {
		switch (state) {
			case S0:	if (*it == 'n') state = S1;
						else goto NULL_END;
						break;
			case S1:	if (*it == 'u') state = S2;
						else goto NULL_END;
						break;
			case S2:	if (*it == 'l') state = S3;
						else goto NULL_END;
						break;
			case S3:	if (*it == 'l') goto NULL_SUCCESS;
						else goto NULL_END;
						break;
		}
	}
	goto NULL_END;

NULL_SUCCESS:
	*ptr = NULL;

NULL_END:
	return ++it;
}

// Forward declaration for recursive function definition
static char* parse_value(char* start, void** val, int64_t* size);

static char* parse_object(char* start, hashtable_t** ht, int64_t* size)
{
	*ht = hashtable_alloc();
	*size = 0;
	enum object_machine_state { S0, S1, S2, S3, S4 };
	enum object_machine_state state = S0;
	char* current_key;
	char* it;
	for (it = start; *it != '\0'; ++it) {
		switch (state) {
			case S0:	if (*it == '{') state = S1;
						else goto OBJ_ERR;
						break;
			case S1:	if (*it == '}') goto OBJ_SUCCESS;
						else if (json_is_whitespace(*it)) state = S1;
						else if (*it == '\"') {
							int64_t size;
							it = parse_string(it, &current_key, &size);
							if (size) {
								--it;
								state = S2;
							}
							else goto OBJ_ERR;
						}
						break;
			case S2:	if (*it == ':') state = S3;
						else if (json_is_whitespace(*it)) state = S2;
						else goto OBJ_ERR;
						break;
			case S3:	if (json_is_whitespace(*it)) state = S3;
						else {
							void* val;
							int64_t size;
							it = parse_value(it, &val, &size);
							if (size) {
								--it;
								hashtable_set(*ht, current_key, val, size);
								current_key = NULL;
								state = S4;
							}
							else goto OBJ_ERR;
						}
						break;
			case S4:	if (*it == ',') state = S1;
						else if (*it == '}') goto OBJ_SUCCESS;
						else if (json_is_whitespace(*it)) state = S4;
						else goto OBJ_ERR;
		}
	}
OBJ_SUCCESS:
	*size = sizeof(hashtable_t);
	return ++it;
OBJ_ERR:
	hashtable_free(*ht);
	return it;
}

static char* parse_array(char* start, list_t** l, int64_t* size)
{
	*l = list_alloc();
	*size = 0;
	enum array_machine_state { S0, S1, S2, S3 };
	enum array_machine_state state = S0;
	char* it;
	for (it = start; *it != '\0'; ++it) {
		int64_t el_size = 0;
		void* el_val = NULL;
		switch (state) {
			case S0:	if (*it == '[') state = S1;
						else goto ARR_ERR;
						break;
			case S1:	if (*it == ']') goto ARR_SUCCESS;
						else if (json_is_whitespace(*it)) state = S1;
						else {
							it = parse_value(it, &el_val, &el_size);
							if (el_size) {
								--it;
								list_add(*l, el_val, el_size);
								state = S2;
							}
							else goto ARR_ERR;
						}
						break;
			case S2:	if (*it == ',') state = S3;
						else if (*it == ']') goto ARR_SUCCESS;
						else if (json_is_whitespace(*it)) state = S2;
						else goto ARR_ERR;
						break;
			case S3:	if (json_is_whitespace(*it)) state = S3;
						else {
							it = parse_value(it, &el_val, &el_size);
							if (el_size) {
								--it;
								list_add(*l, el_val, el_size);
								state = S2;
							}
							else goto ARR_ERR;	
						}
		}
	}

ARR_SUCCESS:
	*size = sizeof(list_t);
	return ++it;

ARR_ERR:
	list_free(*l);
ARR_END:
	return it;
}

static char* parse_value(char* start, void** val, int64_t* size)
{
	*val = NULL;
	*size = 0;

	if (*start == '\"') return parse_string(start, (char**)val, size);
	else if (*start == 't' || *start == 'f') return parse_bool(start, (int64_t**)val, size);
	else if (*start == '-' || json_is_dec_digit(*start)) return parse_number(start, val, size);
	else if (*start == 'n') return parse_null(start, val, size);
	else if (*start == '[') return parse_array(start, (list_t**)val, size); 
	else if (*start == '{') return parse_object(start, (hashtable_t**)val, size);

	return start;
}

hashtable_t* json_parse(char* json)
{
	int64_t size;
	hashtable_t* ht;
	while (*json != '{' && *json != '\0') json++;
	parse_object(json, &ht, &size);
	return ht;
}

int json_parse2(char* json, Json_Object** object)
{
	return 0;
}

static int parse_string2(char** ptr, char** value)
{
//static char* parse_string(char* start, char** str, int64_t* size) 
	*value = NULL;

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

		++it;
	}

STR_SUCCESS:
	size_t len = it - *ptr;
	*value = malloc(len);
	memcpy(*value, (*ptr) + 1, len - 1);
	(*value)[len-1] = '\0';

	*ptr = it++;
	return JSON_OK;
}

static int parse_bool2(char** ptr, bool** value)
{
//static char* parse_bool(char* start, int64_t** b, int64_t* size)
	*value = NULL;

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
							*value = true;
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
							**value = false;
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

static int parse_null2(char** ptr, void** value)
//static char* parse_null(char* start, void** ptr, int64_t* size)
{
	*value = NULL;

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
	*ptr = ++it;
	return JSON_OK;
}

static int parse_value2(char** ptr, enum Json_Object_Type* type, void** object);
static int parse_object2(char** ptr, Json_Object** object);

static int parse_array2(char** ptr, Json_Object** object)
//static char* parse_array(char* start, list_t** l, int64_t* size)
{
	*object = malloc(sizeof(Json_Object));
	memset(*object, 0, sizeof(Json_Object));
	Json_Object* last_child = NULL;

	// S0: Expecting [ character
	// S1: Expecting ] character or json value, ignores whitespaces
	// S2: Expecting , or ] characters, ignores whitespace
	// S3: Expecting json value, ignores whitespaces
	enum array_machine_state { S0, S1, S2, S3 };
	enum array_machine_state state = S0;

	int error;
	char* it;
	for (it = *ptr; *it != '\0'; ++it) {
		int64_t el_size = 0;
		void* el_val = NULL;
		switch (state) {

			case S0:	if (*it == '[') state = S1;
						else goto ARR_ERR;
						break;

			case S1:	if (*it == ']') goto ARR_SUCCESS;
						else if (json_is_whitespace(*it)) state = S1;
						else {

							// Parse JSON value
							void* value;
							enum Json_Object_Type type;
							if ((error = parse_value2(&it, &type, &value))) 
								goto ARR_ERR;

							// Populate node
							Json_Object* child;
							if (type == JSON_ARRAY || JSON_OBJECT) {
								child = (Json_Object*)(value);
							}
							else {
								child = malloc(sizeof(Json_Object));
								memset(child, 0, sizeof(Json_Object));
							}
							child->type = type;
							switch (type) {
								case JSON_INT:
								case JSON_BOOL: child->int_data = (long)value; break;
								case JSON_STRING: child->string_data = (char*)value; break;
								case JSON_DOUBLE: child->double_data = (double)(long)value; break;
							}

							// Insert node into parent
							if (last_child) {
								last_child->next = child;
								child->prev = last_child;
							}
							else {
								(*object)->child = child;
							}
							last_child = child;
							
							--it;
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
							void* value;
							enum Json_Object_Type type;
							if ((error = parse_value2(&it, &type, &value))) 
								goto ARR_ERR;

							// Populate node
							Json_Object* child;
							if (type == JSON_ARRAY || JSON_OBJECT) {
								child = (Json_Object*)value;
							}
							else {
								child = malloc(sizeof(Json_Object));
								memset(child, 0, sizeof(Json_Object));
							}
							child->type = type;
							switch (type) {
								case JSON_INT:
								case JSON_BOOL: child->int_data = (long)value; break;
								case JSON_STRING: child->string_data = (char*)value; break;
								case JSON_DOUBLE: child->double_data = (double)(long)value; break;
							}

							// Insert node into parent
							if (last_child) {
								last_child->next = child;
								child->prev = last_child;
							}
							else {
								(*object)->child = child;
							}
							last_child = child;
							
							--it;
							state = S2;
						}
		}
	}

ARR_ERR:
	Json_Object* p = (*object)->child;
	while (p) {
		Json_Object* temp = p->next;
		free(p);
		p = temp;
	}
	free(*object);
	return JSON_ERROR_ARRAY;

ARR_SUCCESS:
	*ptr = ++it;
	return JSON_OK;
}

static int parse_number2(char** ptr, enum Json_Object_Type* type, void** value)
//static char* parse_number(char* start, void** num, int64_t* size)
{
	*value = NULL;
	*type = JSON_INT;

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
	char* s = malloc(len);
	memcpy(buffer, *ptr, len - 1);
	buffer[len-1] = '\0';
	if (*type == JSON_INT) 
		*value = (void*)atoll(buffer);
	else 
		*value = (void*)(long)atof(buffer);

	*ptr = it;
	return JSON_OK;
}

static int parse_value2(char** ptr, enum Json_Object_Type* type, void** value)
{
//static char* parse_value(char* start, void** val, int64_t* size)
	*value = NULL;

	if (**ptr == '\"') {
		*type = JSON_STRING;
		return parse_string2(ptr, (char**)value);
	}

	if (**ptr == 't' || **ptr == 'f') {
		*type = JSON_BOOL;
		return parse_bool2(ptr, (bool**)value);
	}

	if (**ptr == 'n') {
		*type = JSON_NULL;
		return parse_null2(ptr, value);
	}

	if (**ptr == '[') {
		*type = JSON_ARRAY;
		return parse_array2(ptr, (Json_Object**)value);
	}

	if (**ptr = '{') {
		*type = JSON_OBJECT;
		return parse_object2(ptr, (Json_Object**)value);
	}

	if (**ptr == '-' || json_is_dec_digit(**ptr)) {
		return parse_number2(ptr, type, value);
	}

	// Failed to parse value
	return JSON_ERROR_VALUE;
}

static int parse_object2(char** ptr, Json_Object** object)
{
	*object = malloc(sizeof(Json_Object));
	memset(*object, 0, sizeof(Json_Object));

	// S0: Expecting { character
	// S1: Expecting " character, ignores whitespace
	// S2: Expecting : character, ignores whitespace
	// S3: Expecting JSON value (recursion entrypoint)
	// S4: Expecting , or } character, ignores whitespace
	enum object_machine_state { S0, S1, S2, S3, S4 };
	enum object_machine_state state = S0;

	Json_Object* last_child = NULL;

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
							if ((error = parse_string2(&it, &current_key)))
								goto OBJ_ERR;

							--it;
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
							void* value;
							enum Json_Object_Type type;
							if ((error = parse_value2(&it, &type, &value))) 
								goto OBJ_ERR;

							// Populate node
							Json_Object* child;
							if (type == JSON_ARRAY || JSON_OBJECT) {
								child = (Json_Object*)value;
							}
							else {
								child = malloc(sizeof(Json_Object));
								memset(child, 0, sizeof(Json_Object));
							}
							child->type = type;
							child->key = current_key;
							current_key = NULL;
							switch (type) {
								case JSON_INT:
								case JSON_BOOL: child->int_data = (long)value; break;
								case JSON_STRING: child->string_data = (char*)value; break;
								case JSON_DOUBLE: child->double_data = (double)(long)value; break;
							}

							// Insert node into parent
							if (last_child) {
								last_child->next = child;
								child->prev = last_child;
							}
							else {
								(*object)->child = child;
							}
							last_child = child;

							--it;
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
	*ptr = ++it;
	return JSON_OK;

OBJ_ERR:
	// Do not cleanup individual nodes
	// Object cleanup from error will come from the root
	if (current_key)
		free(current_key);
	return error;
}
