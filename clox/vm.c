#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "value.h"
#include <stdarg.h>
#include <iso646.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VM vm;

static void resetStack() { vm.stackTop = vm.stack; }

static void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);

  vfprintf(stderr, format, args);
  va_end(args);

  fputs("\n", stderr);

  // ip should be n + 1 instructions past vm.chunk->code's pointer.
  size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = vm.chunk->lines[instruction];

  // Get the line of the current instruction
  fprintf(stderr, "[line %d] in script\n", line);
  resetStack();
}

// Not signed perhaps
static Value peek(int distance) {
  // stackTop is next available
  // 0 would be stackTop - 1, as in top value
  // 1 stackTop - 1 - 1
  return *(vm.stackTop - 1 - distance);
}

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  // Already checked types
  // pop string operands off stack
  ObjString* b = AS_STRING(pop());
  ObjString* a = AS_STRING(pop());

  int length = a->length + b->length;

  // +1 for the null terminator
  char* chars = ALLOCATE(char, length + 1);

  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);

  chars[length] = '\0';

  ObjString* result = takeString(chars, length);
  push(OBJ_VAL(result));

  // allocate new string the size of both combined
  // copy a then b and set null terminator
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

// B and A are undefined
#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(0)) && !IS_NUMBER(peek(1))) { \
          runtimeError("Operands must be numbers."); \
          return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
    } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("        ");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_RETURN: {
        printValue(pop());
        printf("\n");
      return INTERPRET_OK;
    }
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant);
      break;
    }
    case OP_NEGATE: {
      if (!IS_NUMBER(peek(0))) {
        runtimeError("Operand must be a number");
        return INTERPRET_RUNTIME_ERROR;
      }

      push(NUMBER_VAL(-AS_NUMBER(pop())));
      break;
    }
    case OP_ADD: {
      // If both operands are strings, concate
      // if both are numbers, add
      // otherwise throw an error
      if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
        concatenate();
      } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a + b));
      } else {
        runtimeError("Operands must be two numbers or two strings.");
        return INTERPRET_RUNTIME_ERROR;
      }

      break;
    }
    case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
    case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
    case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
    case OP_NIL: push(NIL_VAL); break;
    case OP_TRUE: push(BOOL_VAL(true)); break;
    case OP_FALSE: push(BOOL_VAL(false)); break;
    // Read off value, push not of it
    case OP_NOT: push(BOOL_VAL(isFalsey(pop()))); break;
    case OP_EQUAL: {
      Value b = pop();
      Value a = pop();

      push(BOOL_VAL(valuesEqual(a, b)));
      break;
    }
    case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
    case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
    default: {
        printf("Unknown instruction: ");
        exit(1);
    }
    }
  }

#undef BINARY_OP
#undef READ_CONSTANT
#undef READ_BYTE
}

void initVM() { resetStack(); vm.objects = NULL; }

void freeVM() { freeObjects(); }

void push(Value value) {
  // check stack size
  if (vm.stackTop == vm.stack + STACK_MAX) {
    printf("Fatal: popping an empty stack");
    exit(-1);
  }

  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  // Check if stackTop == stack
  if (vm.stackTop == vm.stack) {
    printf("Fatal: popping an empty stack");
    exit(-1);
  }

  vm.stackTop--;
  return *vm.stackTop;
}

InterpretResult interpret(const char* source) {
    Chunk chunk;
    initChunk(&chunk);

    // compile
    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    // place chunk in vm
    // run vm
    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}
