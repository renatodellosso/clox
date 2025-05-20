#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char* argv[])
{
	Chunk chunk;
	initChunk(&chunk);

	// Bytecode is CONSTANT CONSTANT_VALUE (ex: 00 23)
	writeChunk(&chunk, OP_CONSTANT, 123);
	writeChunk(&chunk, addConstant(&chunk, 1.2), 123);

	writeChunk(&chunk, OP_CONSTANT, 124);
	writeChunk(&chunk, addConstant(&chunk, 1.3), 124);

	writeChunk(&chunk, OP_CONSTANT, 124);
	writeChunk(&chunk, addConstant(&chunk, 1.4), 124);

	writeChunk(&chunk, OP_RETURN, 125);

	disassembleChunk(&chunk, "test chunk");

	freeChunk(&chunk);
	return 0;
}