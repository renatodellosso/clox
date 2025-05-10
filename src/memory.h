#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"

// Parentheses around capacity is needed in case it's an expression and we need to avoid order of operations
#define GROW_CAPACITY(capacity) \
 ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount,	newCount) \
	(type*)reallocate(pointer, sizeof(type) * (oldCount), \
		sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
	reallocate(pointer, sizeof(type) * (oldCount), 0)

// size_t holds size of objects. Guaranteed to be large enough to hold any object
// void* is a generic pointer type. Can be implicitly cast to a pointer type
/*
	Old Size / New Size / Operation
	0        / Non-0    / Allocate new block
	Non-0    / 0        / Free allocation
	Non-0    / <oldSize / Shrink allocation
	Non-0    / >oldSize / Grow allocation
*/
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

#endif