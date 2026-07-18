#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "scanner.h"
#include "object.h"
#include "value.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,
  PREC_OR,
  PREC_AND,
  PREC_EQUALITY,
  PREC_COMPARISON,
  PREC_TERM,
  PREC_FACTOR,
  PREC_UNARY,
  PREC_CALL,
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)();

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;


typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

Parser parser;
Chunk* compilingChunk;

static Chunk* currentChunk() {
  return compilingChunk;
}

// "[line <line>] Error at <token>: <message>"
static void errorAt(Token *token, const char *message) {
  // When in panic mode, don't print new error messages until the next statement
  // This avoids a bunch of cascading errors from incomplete syntax
  if (parser.panicMode) return;
  parser.panicMode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  printf("Hello?\n");
  parser.hadError = true;
}

static void error(const char *message) { errorAt(&parser.previous, message); }

static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
}

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR)
      break;

    errorAtCurrent(parser.current.start);
  }
}

static void consume(TokenType expected, const char* message) {
  if (parser.current.type == expected) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitReturn() {
  emitByte(OP_RETURN);
}

// For now, every compiled program ends with an OP_RETURN, to return the expression
// compiled.
static void endCompiler() {
  emitReturn();
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), "code");
  }
#endif

}

static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence prec);

static void binary() {
  // Left is already parsed, and its operations are on the stack and bytecode has already been written
  // operator was just parsed

  TokenType operator = parser.previous.type;

  // Get the precedence of the operator
  ParseRule* rule = getRule(operator);
#ifdef EXTREME_PARSER_TRACE
  printf("Parse binary operator: %d\n", operator);
#endif

  // Parse and emit bytecode for the right operand with the operator's precedence plus one.
  // This makes binary expressions left-associative
  // Ex: If we have 1 + 2 + 3 + 4, the first time we're here we've parsed "1 +".
  // When we recurse with PREC_TERM (for '+') + 1, we want the result to be like
  // (((1 + 2) + 3) + 4)
  //
  // If we didn't add one, the right operand would be grouped on the right
  // (1 + (2 + (3 + 4)))
  parsePrecedence((Precedence)(rule->precedence + 1));

#ifdef EXTREME_PARSER_TRACE
  printf("End parse binary operator: %d\n", operator);
#endif
  switch (operator) {
    case TOKEN_PLUS: emitByte(OP_ADD); break;
    case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
    case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
    case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
    case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
    case TOKEN_GREATER: emitByte(OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
    case TOKEN_LESS: emitByte(OP_LESS); break;
    case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;
    default: return;
  }
}

static uint8_t makeConstant(Value value) {
  // Add to chunk's constant list
  Chunk* chunk = currentChunk();

  // Maybe have this return an unsigned value.
  int constant = addConstant(currentChunk(), value);

  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk.");
  }

  return (uint8_t)constant;
}

static void emitConstant(Value value) {
  // emit OP_CONSTANT with index of new constant.
  uint8_t index = 0;

  emitBytes(OP_CONSTANT, makeConstant(value));
}

static void number() {
  double value = strtod(parser.previous.start, NULL);
#ifdef EXTREME_PARSER_TRACE
  printf("Parse number: %f\n", value);
#endif
  emitConstant(NUMBER_VAL(value));
}

static void parsePrecedence(Precedence prec) {

  advance();

#ifdef EXTREME_PARSER_TRACE
  TokenType prefixType = parser.previous.type;
  printf("[%d] Parse precedence prefix: %d\n", prec, prefixType);
#endif
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;

  if (prefixRule == NULL) {
    error("Expect expression.");
    return;
  }

  // Do prefix function first (number is a prefix itself, so prefix must always happen first)
  prefixRule();
#ifdef EXTREME_PARSER_TRACE
  printf("[%d] End parse precedence prefix: %d\n", prec, prefixType);
#endif

  // While the
  while (prec <= getRule(parser.current.type)->precedence) {
    advance();
#ifdef EXTREME_PARSER_TRACE
    TokenType infixType = parser.previous.type;
    printf("[%d] Parse precedence infix: %d\n", prec, infixType);
#endif
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule();
#ifdef EXTREME_PARSER_TRACE
    printf("End parse precedence infix: %d\n", infixType);
#endif
  }

#ifdef EXTREME_PARSER_TRACE
    printf("[%d] End parse precedence loop: %d\n", prec, getRule(parser.current.type)->precedence);
#endif

}

static void unary() {
  TokenType operatorType = parser.previous.type;

#ifdef EXTREME_PARSER_TRACE
    printf("Parse precedence infix: %d\n", operatorType);
#endif
  // compile expression, only unary and higher (i.e. no binary expressions)
  // This also includes nested unary expressions like !!10
  parsePrecedence(PREC_UNARY);

#ifdef EXTREME_PARSER_TRACE
    printf("End parse precedence infix: %d\n", operatorType);
#endif
  switch(operatorType) {
    case TOKEN_BANG: emitByte(OP_NOT); return;
    case TOKEN_MINUS: emitByte(OP_NEGATE); return;
    default: return;
  }
}

static void grouping() {
#ifdef EXTREME_PARSER_TRACE
  printf("Grouping\n");
#endif
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after exrpession.");
#ifdef EXTREME_PARSER_TRACE
  printf("End grouping\n");
#endif
}

static void literal() {
  switch (parser.previous.type) {
    case TOKEN_NIL: emitByte(OP_NIL); break;
    case TOKEN_FALSE: emitByte(OP_FALSE); break;
    case TOKEN_TRUE: emitByte(OP_TRUE); break;
    default: return;
  }
}

static void string() {
  // previous token is a whole string token.
  // token has a start and a length, which includes leading and trailing '"'

  Value str = OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2));

  emitConstant(str);
}

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]          = {unary,     NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     binary,   PREC_EQUALITY},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary,   PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary,   PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary,   PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,     binary,   PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary,   PREC_COMPARISON},
  [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_STRING]        = {string,     NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,     NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,     NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,     NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static ParseRule* getRule(TokenType type) {
  return &rules[type];
}

static void expression() {
  parsePrecedence(PREC_ASSIGNMENT);
}

bool compile(const char *source, Chunk *chunk) {
  initScanner(source);

  parser.hadError = false;
  parser.panicMode = false;
  compilingChunk = chunk;

  advance();
  expression();
  consume(TOKEN_EOF, "Expect end of expression.");

  endCompiler();
  return !parser.hadError;
}
