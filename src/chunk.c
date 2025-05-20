#include <stdlib.h>
#include "chunk.h"
#include "memory.h"

// No constructors in C
void initChunk(Chunk* chunk)
{
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;

	chunk->lineNumberCapacity = 0;
	chunk->lineNumberCount = 0;
	chunk->lineNumbers = NULL;
	chunk->lineNumberLengths = NULL;

	initValueArray(&chunk->constants); // constants isn't a pointer, so we need & to get the address
}

void freeChunk(Chunk* chunk)
{
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	FREE_ARRAY(int, chunk->lineNumbers, chunk->capacity);
	FREE_ARRAY(int, chunk->lineNumberLengths, chunk->lineNumberCount);
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
	}

	chunk->code[chunk->count] = byte;
	chunk->count++;

	// Update line numbers
	if (chunk->lineNumberCount > 0 && chunk->lineNumbers[chunk->lineNumberCount - 1] == line)
		chunk->lineNumberLengths[chunk->lineNumberCount - 1]++;
	else
	{
		int oldCapacity = chunk->lineNumberCapacity;
		chunk->lineNumberCapacity = GROW_CAPACITY(oldCapacity);

		chunk->lineNumbers = GROW_ARRAY(int, chunk->lineNumbers, oldCapacity, chunk->capacity);
		chunk->lineNumberLengths = GROW_ARRAY(int, chunk->lineNumberLengths, oldCapacity, chunk->capacity);

		chunk->lineNumberCount++;
		chunk->lineNumbers[chunk->lineNumberCount - 1] = line;
		chunk->lineNumberLengths[chunk->lineNumberCount - 1] = 1;
	}
}

int addConstant(Chunk* chunk, Value value)
{
	writeValueArray(&chunk->constants, value);
	return chunk->constants.count - 1; // Use '.' since constants is not a pointer
}

int getLineNumber(Chunk* chunk, int offset)
{
	if (chunk->lineNumberCount == 0)
		return -1;

	int statements = 0;

	for (int i = 0; i < chunk->lineNumberCount; i++)
	{
		if (statements + chunk->lineNumberLengths[i] <= offset)
			statements += chunk->lineNumberLengths[i];
		else
			return chunk->lineNumbers[i];
	}

	return -1;
}