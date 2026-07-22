#include "object.h"
#include "memory.h"
#include "vm.h"
#include "value.h"
#include <stdio.h>
#include <string.h>

#define ALLOCATE_OBJ(type, objectType) \
  (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
  // allocate an a new object of the |size| given
  // |type| is for tagging
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;

  // insert at head of our list of allocated objects
  object->next = vm.objects;
  vm.objects = object;
  return object;
}

// Create a new Obj on the heap with sizeof(ObjString)
// Obj has its type set by allocateObject
// allocateString then sets the string specific fields,
// which works out okay because there is extra space in the alloc'd
// memory after Obj*
static ObjString* allocateString(char* chars, int length) {
  ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  return string;
}

// Doesn't make a copy of the string passed in before placing it in a new ObjString
// allocated struct. Helpful if we already own the string.
ObjString* takeString(char* chars, int length) {
  return allocateString(chars, length);
}

ObjString* copyString(const char* chars, int length) {
  char* heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
  }
}
