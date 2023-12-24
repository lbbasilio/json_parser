#ifndef CUP_HASHTABLE_H
#define CUP_HASHTABLE_H

#include <stdint.h>

#define BUCKETS 11

typedef struct {
	struct hashtable_node_t* next;
	char* key;
	void* data;
	uint64_t data_size;
} hashtable_node_t;

typedef struct {
	hashtable_node_t* buckets[BUCKETS];
	uint64_t bucket_count;
} hashtable_t;

hashtable_t* hashtable_alloc();
void hashtable_free(hashtable_t* ht);
uint64_t hashtable_calc_hash(const char* key);
void* hashtable_get(const hashtable_t* ht, const char* key);
void  hashtable_set(hashtable_t* ht, const char* key, const void* data, uint64_t data_size);

#endif


#ifdef CUP_HASHTABLE_IMPLEMENTATION

uint64_t hashtable_calc_hash(const char* key)
{
	uint64_t hash = 0;
	for(char* it = (char*)key; *it != 0; ++it)
		hash = *it + 31 * hash;
	return hash % BUCKETS;		
}

hashtable_t* hashtable_alloc()
{
	hashtable_t* ht = malloc(sizeof(hashtable_t));
	ht->bucket_count = BUCKETS;
	memset(ht->buckets, 0, sizeof(ht->buckets));
	return ht;
}

void* hashtable_get(const hashtable_t* ht, const char* key)
{
	uint64_t index = hashtable_calc_hash(key);
	hashtable_node_t* node = ht->buckets[index];
	while (node) {
		if (!strcmp(key, node->key))
			return node->data;
		node = node->next;
	}

	return NULL;
}

// When updating key values, you are responsible for freeing the previous object
void hashtable_set(hashtable_t* ht, const char* key, const void* data, uint64_t data_size)
{
	uint64_t index = hashtable_calc_hash(key);
	hashtable_node_t* last = NULL;
	hashtable_node_t* node = ht->buckets[index];

	// Search for key
	while (node) {
		if (!strcmp(key, node->key)) {
			// Key found, replace data
			goto CUP_HASHTABLE_UPDATE;
		}
		last = node;
		node = node->next;
	}

	// Add if not exists
	node = malloc(sizeof(hashtable_node_t));
	node->next = NULL;
	node->key = malloc(strlen(key));
	strcpy(node->key, key);

	if (last) last->next = node;
	else ht->buckets[index] = node;

CUP_HASHTABLE_UPDATE:
	node->data_size = data_size;
	node->data = malloc(data_size);
	memcpy(node->data, data, data_size);
}

void hashtable_free(hashtable_t* ht)
{
	hashtable_node_t* previous;
	hashtable_node_t* current;
	for (uint64_t i = 0; i < ht->bucket_count; ++i) {
		current = ht->buckets[i];
		while (current) {
			previous = current;
			current = current->next;
			free(previous);
		}
	}
	free(ht);
}
#endif
