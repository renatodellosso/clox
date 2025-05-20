#include <stdio.h>

#include "debug.h"
#include "value.h"

void disassembleChunk(Chunk* chunk, const char* name)
{
	printf("== %s ==\n", name);

	for (int offset = 0; offset < chunk->count;)
	{
		offset = disassembleInstruction(chunk, offset);
	}
}

int disassembleInstruction(Chunk* chunk, int offset)
{
	// %04d makes it 4 characters wide
	printf("%04d ", offset);

	// Print line #
	if (offset > 0 && getLineNumber(chunk, offset) == getLineNumber(chunk, offset - 1))
	{
		printf("	| ");
	}
	else
	{
		printf("%4d ", getLineNumber(chunk, offset));
	}

	// Print instruction
	uint8_t	instruction = chunk->code[offset];
	switch (instruction)
	{
	case OP_CONSTANT:
		return constantInstruction("OP_CONSTANT", chunk, offset);
	case OP_RETURN:
		return simpleInstruction("OP_RETURN", offset);
	default:
		printf("Unknown opcod %d\n", instruction);
		return offset + 1;
	}
}

// static funcs can only be accessed in the file they are defined in
static int simpleInstruction(const char* name, int offset)
{
	printf("%s\n", name);
	return offset + 1;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset)
{
	uint8_t	constant = chunk->code[offset + 1]; // Value is next in bytecode

	printf("%-16s %4d '", name, constant); // -16s is a 16-char, left-aligned string
	printValue(chunk->constants.values[constant]);
	printf("'\n");

	return offset + 2;
}