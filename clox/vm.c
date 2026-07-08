#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "value.h"
#include <iso646.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

VM vm;

static void resetStack() { vm.stackTop = vm.stack; }

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op) \
    do { \
        double b = pop(); \
        double a = pop(); \
        push(a op b); \
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
      push(-pop());
      break;
    }
    case OP_ADD: BINARY_OP(+); break;
    case OP_SUBTRACT: BINARY_OP(-); break;
    case OP_MULTIPLY: BINARY_OP(*); break;
    case OP_DIVIDE: BINARY_OP(/); break;
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

void initVM() { resetStack(); }

void freeVM() {}

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
    compile(source);
    return INTERPRET_OK;
  // vm.chunk = chunk;
  // vm.ip = vm.chunk->code;
  // return run();
}
