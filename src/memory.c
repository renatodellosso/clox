#include <stdlib.h>

#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t oldSize, size_t newSize)
{
	vm.bytesAllocated += newSize - oldSize;
	if (newSize > oldSize)
	{
		// Only run if allocating more memory - don't trigger GC when GC frees memory
#ifdef DEBUG_STRESS_GC
		collectGarbage();
#endif
	}

	if (vm.bytesAllocated > vm.nextGC)
	{
		collectGarbage();
		vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;
	}

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

static void freeObject(Obj* object)
{
#ifdef DEBUG_LOG_GC
	printf("%p free type %d\n", (void*)object, object->type);
#endif

	switch (object->type)
	{
	case OBJ_BOUND_METHOD:
		FREE(ObjBoundMethod, object);
		break;
	case OBJ_CLASS:
	{
		ObjClass* klass = (ObjClass*)object;
		freeTable(&klass->methods);
		FREE(ObjClass, object);
		break;
	}
	case OBJ_CLOSURE:
	{
		FREE(ObjClosure, object);
		ObjClosure* closure = (ObjClosure*)object;
		FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
		break;
	}
	case OBJ_FUNCTION:
	{
		ObjFunction* function = (ObjFunction*)object;
		freeChunk(&function->chunk);
		FREE(ObjFunction, object);
		break;
	}
	case OBJ_INSTANCE:
	{
		ObjInstance* instance = (ObjInstance*)object;
		freeTable(&instance->fields);
		FREE(ObjInstance, object);
		break;
	}
	case OBJ_NATIVE:
	{
		FREE(ObjNative, object);
		break;
	}
	case OBJ_STRING:
	{
		ObjString* string = (ObjString*)object;
		FREE_ARRAY(char, string->chars, string->length + 1);
		FREE(ObjString, object);
		break;
	}
	case OBJ_UPVALUE:
		FREE(ObjUpvalue, object);
		break;
	}
}

void markObject(Obj* object)
{
	if (object == NULL)
		return;
	if (object->isMarked)
		return;

#ifdef DEBUG_LOG_GC
	printf("%p mark ", (void*)object);
	printValue(OBJ_VAL(object));
	printf("n");
#endif

	object->isMarked = true;

	if (vm.grayCapacity < vm.grayCount + 1)
	{
		vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
		vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity); // Use C's realloc so it's not managed by the GC

		if (vm.grayStack == NULL)
			exit(1); // realloc failed
	}

	vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value)
{
	// Check for heap values
	if (IS_OBJ(value))
		markObject(AS_OBJ(value));
}

static void markArray(ValueArray* array)
{
	for (int i = 0; i < array->count; i++)
	{
		markValue(array->values[i]);
	}
}

static void	blackenObject(Obj* object)
{
#ifdef DEBUG_LOG_GC
	printf("%p blacken ", (void*)object);
	printValue(OBJ_VAL(object));
	printf("\n");
#endif

	switch (object->type)
	{
	case OBJ_BOUND_METHOD:
	{
		ObjBoundMethod* bound = (ObjBoundMethod*)object;
		markValue(bound->receiver);
		markObject((Obj*)bound->method);
		break;
	}
	case OBJ_CLASS:
	{
		ObjClass* klass = (ObjClass*)object;
		markObject((Obj*)klass->name);
		markTable(&klass->methods);
		break;
	}
	case OBJ_CLOSURE:
	{
		ObjClosure* closure = (ObjClosure*)object;
		markObject((Obj*)closure->function);
		for (int i = 0; i < closure->upvalueCount; i++)
		{
			markObject((Obj*)closure->upvalues[i]);
		}
	}
	case OBJ_FUNCTION:
	{
		ObjFunction* function = (ObjFunction*)object;
		markObject((Obj*)function->name);
		markArray(&function->chunk.constants);
		break;
	}
	case OBJ_INSTANCE:
	{
		ObjInstance* instance = (ObjInstance*)object;
		markObject((Obj*)instance->klass);
		markTable(&instance->fields);
		break;
	}
	case OBJ_UPVALUE:
		markValue(((ObjUpvalue*)object)->closed);
		break;
	case OBJ_NATIVE:
	case OBJ_STRING:
		break;
	}
}

static void markRoots()
{
	for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
	{
		markValue(*slot);
	}

	for (int i = 0; i < vm.frameCount; i++)
	{
		markObject((Obj*)vm.frames[i].closure);
	}

	for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue->next)
	{
		markObject((Obj*)upvalue);
	}

	markTable(&vm.globals);
	markCompilerRoots();
	markObject((Obj*)vm.initString);
}

static void traceReferences()
{
	while (vm.grayCount > 0)
	{
		Obj* object = vm.grayStack[--vm.grayCount];
		blackenObject(object);
	}
}

static void sweep()
{
	Obj* previous = NULL;
	Obj* object = vm.objects;

	while (object != NULL)
	{
		if (object->isMarked)
		{
			object->isMarked = false;
			previous = object;
			object = object->next;
		}
		else
		{
			Obj* unreached = object;
			object = object->next;
			if (previous != NULL)
			{
				previous->next = object;
			}
			else
			{
				vm.objects = object;
			}

			freeObject(unreached);
		}
	}
}

void collectGarbage()
{
#ifdef 	DEBUG_LOG_GC
	printf("-- gc begin\n");
	size_t before = vm.bytesAllocated;
#endif

	markRoots();
	traceReferences();
	tableRemoveWhite(&vm.strings);
	sweep();

#ifdef 	DEBUG_LOG_GC
	printf("-- gc end\n");
	printf("\tcollected %zu bytes (from %zu to %zu) next at %zu\n", before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
#endif
}

void freeObjects()
{
	Obj* object = vm.objects;
	while (object != NULL)
	{
		Obj* next = object->next;
		freeObject(object);
		object = next;
	}

	free(vm.grayStack);
}