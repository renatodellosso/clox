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
	Obj* objects;
} VM;

typedef enum
{
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

// Expose vm to other files
extern VM vm;

void initVM();
void freeVM();
InterpretResult	interpret(const char* source);
void push(Value value);
Value pop();

#endif