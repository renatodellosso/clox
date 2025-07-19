#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef	struct
{
	Chunk* chunk;
	uint8_t* ip; // uint8_t is a byte, so this points to the byte of bytecode that will be executed next
	Value stack[STACK_MAX];
	Value* stackTop; // Points to where the next value to be pushed will go
} VM;

typedef enum
{
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM();
void freeVM();
InterpretResult	interpret(Chunk* chunk);
void push(Value value);
Value pop();

#endif