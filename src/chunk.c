#include <stdlib.h>
#include "chunk.h"
#include "memory.h"
#include <stdio.h>

// No constructors in C
void initChunk(Chunk* chunk)
{
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
	chunk->lines = NULL;
	initValueArray(&chunk->constants); // constants isn't a pointer, so we need & to get the address
}

void freeChunk(Chunk* chunk)
{
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	FREE_ARRAY(int, chunk->lines, chunk->capacity);
	freeValueArray(&chunk->constants);
	initChunk(chunk); // Zero-out the chunk to ensure the state is defined
}

// Use uint8_t (8-bit unsigned) - it's smaller
void writeChunk(Chunk* chunk, uint8_t byte, int line)
{
	if (chunk->capacity < chunk->count + 1)
	{
		int oldCapacity = chunk->capacity;
		chunk->capacity = GROW_CAPACITY(oldCapacity);
		chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
		chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
	}

	chunk->code[chunk->count] = byte;
	chunk->lines[chunk->count] = line;
	chunk->count++;
}

int addConstant(Chunk* chunk, Value value)
{
	writeValueArray(&chunk->constants, value);
	return chunk->constants.count - 1; // Use '.' since constants is not a pointer
}

void writeConstant(Chunk* chunk, Value value, int line)
{
	int constant = addConstant(chunk, value);

	if (constant < 256)
	{
		writeChunk(chunk, OP_CONSTANT, line);
		writeChunk(chunk, constant, line);
	}
	else
	{
		writeChunk(chunk, OP_CONSTANT_LONG, line);
		for (int i = 1; i >= 0; i--)
		{
			writeChunk(chunk, (constant >> (8 * i)) & 0xff, line);
		}
	}
}