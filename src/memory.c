#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include <stdio.h>

typedef struct
{
	int size;
	int free; // 0 = no block, 1 = free, 2 = unfree
} MemBlock;

void* start = NULL;

void* reallocate(void* pointer, size_t oldSize, size_t newSize)
{
	if (start == NULL)
		start = calloc(1000000, 1);

	// If pointer is NULL, return new block of newSize
	// Otherwise, find pointer, copy its data to a block of newSize, and free the old block

	MemBlock* oldBlock = pointer != NULL ? (int*)pointer - 1 : NULL;
	printf("PTR -> Old Block: %p -> %p\n", pointer, oldBlock);

	if (newSize == 0)
	{
		if (oldBlock)
		{
			oldBlock->free = 1;
		}

		return;
	}

	MemBlock* curr = start;

	if (curr->free != 0)
	{
		printf("Initial block is not free...\n");
		while (curr != NULL && (curr->free == 2 || curr->size < newSize))
		{
			curr += curr->size;
			printf("Checking %p. Size: %d. Free: %d", curr - start, curr->size, curr->free);
		}
		printf("Found suitable block at %p. Size: %d\n", curr - start, curr->size);

		// Create a new block out of the unneeded space
		MemBlock* newBlock = curr + newSize / sizeof(MemBlock);
		newBlock->size = curr->size - newSize - sizeof(MemBlock); // Remember space for metadata!
		newBlock->free = 1;
	}

	// Found space
	curr->free = 2;
	curr->size = newSize;

	printf("Curr: %p\n", curr - start);

	if (oldBlock)
	{
		printf("Reallocating...\n");
		oldBlock->free = 1;
		memcpy(pointer, curr + 1, oldSize);
	}

	return curr;
}

void* reallocateOld(void* pointer, size_t oldSize, size_t newSize)
{
	if (newSize == 0)
	{
		// Deallocate
		free(pointer);
		return NULL;
	}

	// realloc expands the pointer's area if possible. If not possible, it will copy pointer and its data to a new area of the correct size
	// If data is copied, realloc returns the new pointer
	// Returns NULL if there is not enough memory
	void* result = realloc(pointer, newSize);
	if (result == NULL)
		exit(1);
	return result;
}