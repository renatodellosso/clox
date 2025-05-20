#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// typedef creates an alias. OpCode is now enum { ... }
typedef	enum
{
	OP_CONSTANT,
	OP_RETURN
} OpCode;

typedef	struct
{
	int count;
	int capacity;
	uint8_t* code;
	ValueArray constants;

	int lineNumberCapacity;
	int lineNumberCount;
	int* lineNumbers;
	int* lineNumberLengths;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);
int getLineNumber(Chunk* chunk, int offset);

#endif