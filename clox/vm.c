#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include <iso646.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

VM vm;

static Value clockNative(int argCount, Value *args) {
  UNUSED(argCount);
  UNUSED(args);

  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);

  vfprintf(stderr, format, args);
  va_end(args);

  fputs("\n", stderr);

  // Stack trace means go from current frame back to first frame and print
  // function and line.
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm.frames[i];
    ObjFunction *func = frame->closure->function;

    size_t instruction = frame->ip - frame->closure->function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", func->chunk.lines[instruction]);
    if (func->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", func->name->chars);
    }
  }
  resetStack();
}

static void defineNative(const char *name, NativeFn function) {
  // Pushing these onto the stack is for future GC reasons.
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

// create new callstack, set ip to func chunk code
static bool call(ObjClosure *closure, uint8_t numArgs) {
  if (numArgs != closure->function->arity) {
    runtimeError("Expected %d arguments for %.*s, but got %d",
                 closure->function->arity, closure->function->name->length,
                 closure->function->name->chars, numArgs);
    return false;
  }

  if (vm.frameCount == FRAME_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount++];

  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stackTop - numArgs - 1;

  return true;
}

static bool callValue(Value callee, uint8_t numArgs) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
    case OBJ_CLOSURE:
      return call(AS_CLOSURE(callee), numArgs);
    case OBJ_NATIVE: {
      NativeFn func = AS_NATIVE(callee);
      Value result = func(numArgs, vm.stackTop - numArgs);

      vm.stackTop -= numArgs + 1;
      push(result);
      return true;
    }
    case OBJ_CLASS: {
      ObjClass *klass = AS_CLASS(callee);

      // Need to create a new instance and push it onto the stack.
      ObjInstance *instance = newInstance(klass);
      vm.stackTop[-numArgs - 1] = OBJ_VAL(instance);
      return true;
    }
    default:
      break;
    }
  }

  runtimeError("Cannot call non function");
  return false;
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

// Uses insertion sort to sort by local pointer.
static ObjUpvalue *captureUpvalue(Value *local) {
  ObjUpvalue *prevUpvalue = NULL;
  ObjUpvalue *upvalue = vm.openUpvalues;

  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  // Upvalue not found, create a new one.

  ObjUpvalue *createdUpvalue = newUpvalue(local);
  createdUpvalue->next = upvalue;

  // If there is no previous, the new head is the createdUpvalue.
  if (prevUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

/**
 * vm.openUpvalues are sorted highest location address first, and
 * closeUpvalues should be called on the function whose variables are at the
 * top of the stack and about to be cleaned up.
 *
 * This is why we start at the head and advance the head pointer and
 * stop once a upvalue location pointer is less than |last|.
 */
static void closeUpvalues(Value *last) {
  while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
    ObjUpvalue *upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}

static void concatenate() {
  // Already checked types
  // pop string operands off stack
  ObjString *b = AS_STRING(peek(0));
  ObjString *a = AS_STRING(peek(1));

  int length = a->length + b->length;

  // +1 for the null terminator
  char *chars = ALLOCATE(char, length + 1);

  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);

  chars[length] = '\0';

  ObjString *result = takeString(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));

  // allocate new string the size of both combined
  // copy a then b and set null terminator
}

static InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_SHORT()                                                           \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT()                                                        \
  (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#ifdef DEBUG_TRACE_EXECUTION
  printf("\nBegin execution\n");
#endif

// B and A are undefined
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) && !IS_NUMBER(peek(1))) {                          \
      runtimeError("Operands must be numbers.");                               \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop());                                               \
    double a = AS_NUMBER(pop());                                               \
    push(valueType(a op b));                                                   \
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
    disassembleInstruction(
        &frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_RETURN: {
      Value value = pop();
      closeUpvalues(frame->slots);
      vm.frameCount--;
      if (vm.frameCount == 0) {
        pop();
        return INTERPRET_OK;
      }

      vm.stackTop = frame->slots;
      push(value);
      frame = &vm.frames[vm.frameCount - 1];

      break;
    }
    case OP_PRINT: {
      // Pop off statement and print value
      Value val = pop();
      printValue(val);
      printf("\n");
      break;
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
    case OP_SUBTRACT:
      BINARY_OP(NUMBER_VAL, -);
      break;
    case OP_MULTIPLY:
      BINARY_OP(NUMBER_VAL, *);
      break;
    case OP_DIVIDE:
      BINARY_OP(NUMBER_VAL, /);
      break;
    case OP_NIL:
      push(NIL_VAL);
      break;
    case OP_TRUE:
      push(BOOL_VAL(true));
      break;
    case OP_FALSE:
      push(BOOL_VAL(false));
      break;
    // Read off value, push not of it
    case OP_NOT:
      push(BOOL_VAL(isFalsey(pop())));
      break;
    case OP_EQUAL: {
      Value b = pop();
      Value a = pop();

      push(BOOL_VAL(valuesEqual(a, b)));
      break;
    }
    case OP_LESS:
      BINARY_OP(BOOL_VAL, <);
      break;
    case OP_GREATER:
      BINARY_OP(BOOL_VAL, >);
      break;
    case OP_DEFINE_GLOBAL: {
      ObjString *name = READ_STRING();
      tableSet(&vm.globals, name, peek(0));
      pop();
      break;
    }
    case OP_GET_GLOBAL: {
      ObjString *name = READ_STRING();
      Value value;
      if (!tableGet(&vm.globals, name, &value)) {
        runtimeError("Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      push(value);

      break;
    }
    case OP_SET_GLOBAL: {
      // Read string does the following
      // 1. Reads the value pointed to by the vm.ip pointer (the index in the
      // constants table)
      // 2. Uses that number to index into the current chunk's constants array's
      // values
      // 3. Grabs the obj part of the union type and cast to ObjString
      ObjString *name = READ_STRING();
      if (tableSet(&vm.globals, name, peek(0))) {
        tableDelete(&vm.globals, name);
        runtimeError("Undefined variable '%s'", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      break;
    }
    case OP_GET_LOCAL: {
      // Read slot value from next byte
      uint8_t slot = READ_BYTE();
      push(frame->slots[slot]);
      break;
    }
    case OP_SET_LOCAL: {
      // Read slot value from next byte
      uint8_t slot = READ_BYTE();
      frame->slots[slot] = peek(0);
      break;
    }
    case OP_GET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      push(*frame->closure->upvalues[slot]->location);

      break;
    }
    case OP_SET_UPVALUE: {
      uint8_t slot = READ_BYTE();

      *frame->closure->upvalues[slot]->location = peek(0);
      break;
    }
    case OP_POP: {
      // Pop value off the stack
      pop();
      break;
    }
    case OP_JUMP: {
      // Read two bytes and then increase the ip by that much
      uint16_t offset = READ_SHORT();
      frame->ip += offset;
      break;
    }
    case OP_JUMP_IF_FALSE: {
      // Read two bytes and then increase the ip by that much
      uint16_t offset = READ_SHORT();

      if (isFalsey(peek(0)))
        frame->ip += offset;

      break;
    }
    case OP_LOOP: {
      uint16_t offset = READ_SHORT();
      frame->ip -= offset;
      break;
    }
    case OP_CALL: {
      uint8_t numArgs = READ_BYTE();
      if (!callValue(peek(numArgs), numArgs)) {
        return INTERPRET_RUNTIME_ERROR;
      }

      // update local cached pointer, since callValue can
      // add a new CallFrame
      frame = &vm.frames[vm.frameCount - 1];
      break;
    }
    case OP_CLOSURE: {
      // read byte
      ObjFunction *func = AS_FUNCTION(READ_CONSTANT());
      ObjClosure *closure = newClosure(func);

      push(OBJ_VAL(closure));

      // Read pairs of bytes for the amount of upvalues
      for (int i = 0; i < closure->upvalueCount; i++) {
        uint8_t isLocal = READ_BYTE();
        uint8_t index = READ_BYTE();

        if (isLocal) {
          // If it's local we need to grab it from the stack.
          closure->upvalues[i] = captureUpvalue(frame->slots + index);
        } else {
          // If it's not local, we can just grab it off the enclosing
          // frame's upvalues.
          closure->upvalues[i] = frame->closure->upvalues[index];
        }
      }

      break;
    }
    case OP_CLOSE_UPVALUE: {
      closeUpvalues(vm.stackTop - 1);
      pop();
      break;
    }
    // Creates a new class with the name from the string operand
    case OP_CLASS: {
      ObjString *name = READ_STRING();
      push(OBJ_VAL(newClass(name)));

      break;
    }
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

void initVM() {
  resetStack();
  vm.objects = NULL;
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;
  initTable(&vm.globals);
  initTable(&vm.strings);

  vm.grayCapacity = 0;
  vm.grayCount = 0;
  vm.grayStack = NULL;

  defineNative("clock", clockNative);
}

void freeVM() {
  freeObjects();
  freeTable(&vm.globals);
  freeTable(&vm.strings);
}

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

InterpretResult interpret(const char *source) {
  ObjFunction *func = compile(source);

  // compile
  if (func == NULL) {
    return INTERPRET_COMPILE_ERROR;
  }

  // Push this object to the stack, the push/pop shenanigans is for upcoming GC.
  push(OBJ_VAL(func));
  ObjClosure *closure = newClosure(func);
  pop();
  push(OBJ_VAL(closure));

  // Create a new callstack
  call(closure, 0);
  return run();
}
