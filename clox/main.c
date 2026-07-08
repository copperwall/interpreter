#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"
#include <iso646.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG_TRACE_EXECUTION

static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }


}

static char* readFile(const char* filename) {
    FILE* file = fopen("filename", "rb");

    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", filename);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);

    rewind(file);

    // Plus one for the null byte.
    char* sourceBuf = (char *)malloc(fileSize + 1);

    if (sourceBuf == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", filename);
        exit(74);
    }

    size_t bytesRead = fread(sourceBuf, sizeof(char), fileSize, file);
    sourceBuf[bytesRead] = '\0';

    fclose(file);
    return sourceBuf;
}

static void runFile(const char* filename) {
    char* source = readFile(filename);

    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}


int main(int argc, const char *argv[]) {
  initVM();

  if (argc == 1) {
      repl();
  } else if (argc == 2) {
      runFile(argv[1]);
  } else {
      fprintf(stderr, "Usage: clox [path]\n");
      exit(64);
  }

  freeVM();

  return 0;
}
