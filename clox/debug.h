#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* str);
int disassembleInstruction(Chunk* chunk, int offset);

#endif
