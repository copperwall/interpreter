#include "object.h"
#include "chunk.h"
#include "common.h"
#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALLOCATE_OBJ(type, objectType)                                         \
  (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjType type) {
  // allocate an a new object of the |size| given
  // |type| is for tagging
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;
  object->isMarked = false;

  // insert at head of our list of allocated objects
  object->next = vm.objects;
  vm.objects = object;

#ifdef DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void *)object, size, type);
#endif
  return object;
}

ObjFunction *newFunction() {
  ObjFunction *objFunction = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  objFunction->arity = 0;
  objFunction->name = NULL;
  initChunk(&objFunction->chunk);

  return objFunction;
}

ObjClosure *newClosure(ObjFunction *function) {
  ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue *, function->upvalueCount);

  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

  ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

ObjUpvalue *newUpvalue(Value *slot) {
  ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->location = slot;
  upvalue->next = NULL;
  upvalue->closed = NIL_VAL;

  return upvalue;
}

ObjNative *newNative(NativeFn function) {
  ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);

  native->function = function;
  return native;
}

static void printFunction(ObjFunction *func) {
  if (func->name == NULL) {
    printf("<script>");
    return;
  }

  printf("<fn %s>", func->name->chars);
}

// Create a new Obj on the heap with sizeof(ObjString)
// Obj has its type set by allocateObject
// allocateString then sets the string specific fields,
// which works out okay because there is extra space in the alloc'd
// memory after Obj*
static ObjString *allocateString(char *chars, int length, uint32_t hash) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  // Push onto value stack so that it is marked as a root and not GC'd
  // before being in the string table.
  push(OBJ_VAL(string));
  tableSet(&vm.strings, string, NIL_VAL);
  pop();
  return string;
}

// FNV-1a hash algorithm
static uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;

  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }

  return hash;
}

// Doesn't make a copy of the string passed in before placing it in a new
// ObjString allocated struct. Helpful if we already own the string.
ObjString *takeString(char *chars, int length) {
  uint32_t hash = hashString(chars, length);

  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);

  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }
  return allocateString(chars, length, length);
}

ObjString *copyString(const char *chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL)
    return interned;

  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  case OBJ_FUNCTION:
    printFunction(AS_FUNCTION(value));
    break;
  case OBJ_NATIVE:
    printf("<native fn>");
    break;
  case OBJ_CLOSURE:
    printFunction(AS_CLOSURE(value)->function);
    break;
  case OBJ_UPVALUE:
    printf("upvalue");
    break;
  }
}
