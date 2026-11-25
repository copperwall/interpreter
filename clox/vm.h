#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "common.h"
#include "object.h"
#include "table.h"
#include <stdint.h>

#define FRAME_MAX 64
#define STACK_MAX (FRAME_MAX * UINT8_COUNT)

typedef struct {
  // Reference to the function being invoked.
  ObjClosure *closure;
  // ip to return to after the function is done.
  uint8_t *ip;
  // Where in the stack to access locals.
  Value *slots;
} CallFrame;

typedef struct {
  // CallFrame handles the IP and code chunk
  CallFrame frames[FRAME_MAX];
  int frameCount;
  Value stack[STACK_MAX];
  // 1 past the end of the stack
  // Empty stack is stackTop == &stack
  Value *stackTop;
  Table globals;
  Table strings;
  ObjString *initString;
  ObjUpvalue *openUpvalues;

  size_t bytesAllocated;
  size_t nextGC;
  Obj *objects;
  int grayCount;
  int grayCapacity;
  Obj **grayStack;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char *source);
void push(Value value);
Value pop();

#endif
