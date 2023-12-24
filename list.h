#ifndef CUP_LIST_H
#define CUP_LIST_H

#include <stdint.h>

typedef struct {
	struct list_node_t* next;
	uint64_t data_size;
	void* data;
} list_node_t;

typedef struct {
	list_node_t* head;
	uint64_t count;
} list_t;

list_t* list_alloc();
void list_free(list_t* l);

void* list_get(const list_t* l, uint64_t index, uint64_t* data_size);
void list_set(const list_t* l, uint64_t index, const void* data, uint64_t data_size);
void list_add(list_t* l, const void* data, uint64_t data_size);
void* list_pop(list_t* l, uint64_t* data_size);

#endif


#ifdef CUP_LIST_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>

list_t*	list_alloc()
{
	list_t* l = malloc(sizeof(list_t));
	if (l) memset(l, 0, sizeof(list_t));
	return l;
}

void list_free(list_t* l)
{
	list_node_t* it = l->head;
	list_node_t* tmp = NULL;
	while (it) {
		tmp = it;
		it = it->next;
		free(tmp);
	}
	free(l);
}

void* list_get(const list_t* l, uint64_t index, uint64_t* data_size)
{
	*data_size = 0;
	if (index >= l->count) return NULL;

	list_node_t* node = l->head;
	for (uint64_t it = 0; it < index; ++it)
		node = node->next;
	
	*data_size = node->data_size;
	return node->data;
}

void list_set(const list_t* l, uint64_t index, const void* data, uint64_t data_size)
{
	if (index >= l->count) return;

	list_node_t* node = l->head;
	for (uint64_t it = 0; it < index; ++it)
		node = node->next;

	free(node->data);
	node->data_size = data_size;
	node->data = malloc(data_size);
	memcpy(node->data, data, data_size);
}

void list_add(list_t* l, const void* data, uint64_t data_size)
{
	list_node_t* node = l->head;
	while (node && node->next) node = node->next; 
	l->count++;

	list_node_t* new_node = malloc(sizeof(list_node_t));
	memset(new_node, 0, sizeof(list_node_t));
	new_node->data_size = data_size;
	new_node->data = malloc(data_size);
	memcpy(new_node->data, data, data_size);

	if (node) node->next = new_node;
	else l->head = new_node;
}

void* list_pop(list_t* l, uint64_t* data_size)
{
	*data_size = 0;
	if (!l->count) return NULL;
	l->count--;

	list_node_t* new_end = NULL;
	list_node_t* node = l->head;
	while (node->next) {
		new_end = node;
		node = node->next;
	}

	void* data = malloc(node->data_size);
	memcpy(data, node->data, node->data_size);
	*data_size = node->data_size;

	if (new_end) new_end->next = NULL;
	else l->head = NULL;

	return data;
}

#endif
