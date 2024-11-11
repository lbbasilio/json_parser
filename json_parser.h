#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stdbool.h>

#define JSON_OK 			0x0
#define JSON_ERROR_VALUE 	0x1
#define JSON_ERROR_STRING 	0x2
#define JSON_ERROR_BOOL 	0x3
#define JSON_ERROR_NULL		0x4
#define JSON_ERROR_ARRAY	0x5
#define JSON_ERROR_OBJECT	0x6
#define JSON_ERROR_NUMBER	0x7

typedef struct Json_Node Json_Node;
typedef enum Json_Node_Type Json_Node_Type;
typedef union Json_Node_Value Json_Node_Value;

enum Json_Node_Type
{ 
	JSON_UNKNOWN, 
	JSON_NULL, 
	JSON_BOOL, 
	JSON_ARRAY, 
	JSON_OBJECT, 
	JSON_LONG, 
	JSON_DOUBLE, 
	JSON_STRING
};

union Json_Node_Value
{
	long long_data;
	bool bool_data;
	char* string_data;
	double double_data;
	Json_Node* child;
};

struct Json_Node
{
	char* key;
	Json_Node* next;
	Json_Node* prev;

	Json_Node_Type type;
	Json_Node_Value value;
};

int json_parse2(char* json, Json_Node** object);
Json_Node* json_get(Json_Node* object, char* key);
void json_delete(Json_Node* json);

#endif
