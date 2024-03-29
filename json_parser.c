#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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
