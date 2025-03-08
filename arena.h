// Simple arena allocator - v1.1

#ifndef ARENA_H_
#define ARENA_H_

#include <stdlib.h>

typedef struct {
	unsigned char* base;
	unsigned char* head;
	size_t capacity;
} Arena;

Arena* arena_create(size_t capacity);
void* arena_alloc(Arena* arena, size_t bytes);
unsigned char* arena_checkpoint(Arena* arena);
void arena_rollback(Arena* arena, unsigned char* ptr);
void arena_destroy(Arena* arena);

#endif

#ifdef ARENA_IMPLEMENTATION

Arena* arena_create(size_t capacity)
{
	Arena* arena = malloc(sizeof(Arena));
	if (arena == NULL)
		return NULL;

	arena->capacity = capacity;
	arena->base = malloc(capacity);
	arena->head = arena->base;
	if (arena->base == NULL) {
		free(arena);
		return NULL;
	}

	return arena;
}

void* arena_alloc(Arena* arena, size_t bytes)
{
	if (arena == NULL || arena->base == NULL || bytes == 0)
		return NULL;

	if (bytes > arena->capacity - (arena->head - arena->base))
		return NULL;

	void* ptr = arena->head;
	arena->head += bytes;
	return ptr;
}

unsigned char* arena_checkpoint(Arena* arena)
{
	return arena->head;
}

void arena_rollback(Arena* arena, unsigned char* checkpoint)
{
	if (arena->base >= checkpoint && checkpoint >= arena->base + arena->capacity)
		arena->head = checkpoint;
}

void arena_destroy(Arena* arena) 
{
	if (arena != NULL) {
		if (arena->base != NULL)
			free(arena->base);
		free(arena);
	}
}

#endif
