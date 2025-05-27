#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char* argv[])
{
	Chunk chunk;
	initChunk(&chunk);

	for (int i = 0; i < 264; i++)
		writeConstant(&chunk, i, 0);

	disassembleChunk(&chunk, "test chunk");

	freeChunk(&chunk);
	return 0;
}