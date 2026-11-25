#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NAN_BOXING

// #define EXTREME_PARSER_TRACE

// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION

// Release builds (`make release` / `make bench`, which pass -DNDEBUG) always
// strip the debug output, regardless of which flags are enabled above, so
// benchmarks never end up timing printf.
#ifdef NDEBUG
#undef EXTREME_PARSER_TRACE
#undef DEBUG_PRINT_CODE
#undef DEBUG_TRACE_EXECUTION

#undef DEBUG_STRESS_GC
#endif

#define UINT8_COUNT (UINT8_MAX + 1)

// Used to quiet compiler warnings about unused variables.
#define UNUSED(x) ((void)(x))

#endif
