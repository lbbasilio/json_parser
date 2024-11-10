#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include "hashtable.h"

#define JSON_OK 			0x0
#define JSON_ERROR_VALUE 	0x1
#define JSON_ERROR_STRING 	0x2
#define JSON_ERROR_BOOL 	0x3
#define JSON_ERROR_NULL		0x4
#define JSON_ERROR_ARRAY	0x5
#define JSON_ERROR_OBJECT	0x6
#define JSON_ERROR_NUMBER	0x7

enum Json_Object_Type { JSON_NULL, JSON_BOOL, JSON_ARRAY, JSON_OBJECT, JSON_INT, JSON_DOUBLE, JSON_STRING };
typedef struct Json_Object
{
	char* key;
	struct Json_Object* next;
	struct Json_Object* prev;
	struct Json_Object* child;

	enum Json_Object_Type type;

	union {
		long int_data;
		int array_len;
		char* string_data;
		double double_data;
	};

} Json_Object;

hashtable_t* json_parse(char* json);
int json_parse2(char* json, Json_Object** object);
void json_delete(Json_Object* json);

#endif
