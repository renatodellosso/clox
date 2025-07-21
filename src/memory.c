#include <stdlib.h>

#include "memory.h"
#include "vm.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize)
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

static void freeObject(Obj* object)
{
	switch (object->type)
	{
	case OBJ_STRING:
	{
		ObjString* string = (ObjString*)object;
		FREE_ARRAY(char, string->chars, string->length + 1);
		FREE(ObjString, object);
		break;
	}
	}
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
}