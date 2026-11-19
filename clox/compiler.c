#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"
#include "value.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

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

typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int depth;
  bool isCaptured;
} Local;

typedef struct {
  uint8_t index;
  bool isLocal;
} Upvalue;

typedef enum {
  TYPE_FUNCTION,
  // This is for compiling top-level code, not a function.
  // Compiler always has a ObjFunction reference, so it's helpful to distinguish
  TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
  struct Compiler *enclosing;
  // The current function
  ObjFunction *function;
  FunctionType type;
  Local locals[UINT8_COUNT];
  int localCount;
  Upvalue upvalues[UINT8_COUNT];
  int scopeDepth;
} Compiler;

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

Parser parser;
Compiler *current = NULL;
Chunk *compilingChunk;

static Chunk *currentChunk() { return &current->function->chunk; }

static void initCompiler(Compiler *compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->type = type;
  compiler->function = newFunction();

  if (type == TYPE_FUNCTION) {
    // Copy name into struct from the source/tokenizer.
    compiler->function->name =
        copyString(parser.previous.start, parser.previous.length);
  }
  current = compiler;

  // Reserve the first slot in the locals list.
  Local *local = &current->locals[current->localCount++];
  local->depth = 0;
  local->name.start = "";
  local->name.length = 0;
  local->isCaptured = false;
}

// "[line <line>] Error at <token>: <message>"
static void errorAt(Token *token, const char *message) {
  // When in panic mode, don't print new error messages until the next statement
  // This avoids a bunch of cascading errors from incomplete syntax
  if (parser.panicMode)
    return;
  parser.panicMode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
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

static void consume(TokenType expected, const char *message) {
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
  emitByte(OP_NIL);
  emitByte(OP_RETURN);
}

// For now, every compiled program ends with an OP_RETURN, to return the
// expression compiled.
static ObjFunction *endCompiler() {
  emitReturn();
  ObjFunction *func = current->function;
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(),
                     func->name != NULL ? func->name->chars : "<script>");
  }
#endif

  current = current->enclosing;
  return func;
}

static void expression();
static void block();
static void statement();
static void declaration();

static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence prec);

static void binary(bool canAssign) {
  // Left is already parsed, and its operations are on the stack and bytecode
  // has already been written operator was just parsed

  TokenType operator= parser.previous.type;

  // Get the precedence of the operator
  ParseRule *rule = getRule(operator);
#ifdef EXTREME_PARSER_TRACE
  printf("Parse binary operator: %d\n", operator);
#endif

  // Parse and emit bytecode for the right operand with the operator's
  // precedence plus one. This makes binary expressions left-associative Ex: If
  // we have 1 + 2 + 3 + 4, the first time we're here we've parsed "1 +". When
  // we recurse with PREC_TERM (for '+') + 1, we want the result to be like
  // (((1 + 2) + 3) + 4)
  //
  // If we didn't add one, the right operand would be grouped on the right
  // (1 + (2 + (3 + 4)))
  parsePrecedence((Precedence)(rule->precedence + 1));

#ifdef EXTREME_PARSER_TRACE
  printf("End parse binary operator: %d\n", operator);
#endif
  switch (operator) {
  case TOKEN_PLUS:
    emitByte(OP_ADD);
    break;
  case TOKEN_MINUS:
    emitByte(OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emitByte(OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emitByte(OP_DIVIDE);
    break;
  case TOKEN_BANG_EQUAL:
    emitBytes(OP_EQUAL, OP_NOT);
    break;
  case TOKEN_EQUAL_EQUAL:
    emitByte(OP_EQUAL);
    break;
  case TOKEN_GREATER:
    emitByte(OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emitBytes(OP_LESS, OP_NOT);
    break;
  case TOKEN_LESS:
    emitByte(OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emitBytes(OP_GREATER, OP_NOT);
    break;
  default:
    return;
  }
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
  if (!check(type))
    return false;

  advance();
  return true;
}

static void beginScope() { current->scopeDepth++; }

static void endScope() {
  current->scopeDepth--;

  // pop all local vars from this scope off the stack
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {

    // Captured variables must be handled differently.
    if (current->locals[current->localCount - 1].isCaptured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }
    current->localCount--;
  }
}

static uint8_t makeConstant(Value value) {
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

static int emitJump(OpCode opcode) {
  emitByte(opcode);
  emitByte(0xff);
  emitByte(0xff);

  // This returns the index of the first jump offset byte,
  // since we'll need to patch that later (and not the opcode)
  // -2 is because after emitting three bytes, the count is one ahead
  // of the three bytes emitted.
  return currentChunk()->count - 2;
}

static void patchJump(int offset) {
  int jump = currentChunk()->count - offset - 2;

  // if jump too big, error
  if (jump > UINT16_MAX) {
    error("Jump is too big");
  }

  // overwrite jump offset
  // place most significant bytes first for VM to read in order.
  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static void number(bool canAssign) {
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

  bool canAssign = prec <= PREC_ASSIGNMENT;
  // Do prefix function first (number is a prefix itself, so prefix must always
  // happen first)
  prefixRule(canAssign);
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
    infixRule(canAssign);
#ifdef EXTREME_PARSER_TRACE
    printf("End parse precedence infix: %d\n", infixType);
#endif
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }

#ifdef EXTREME_PARSER_TRACE
  printf("[%d] End parse precedence loop: %d\n", prec,
         getRule(parser.current.type)->precedence);
#endif
}

static void unary(bool canAssign) {
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
  switch (operatorType) {
  case TOKEN_BANG:
    emitByte(OP_NOT);
    return;
  case TOKEN_MINUS:
    emitByte(OP_NEGATE);
    return;
  default:
    return;
  }
}

static void grouping(bool canAssign) {
#ifdef EXTREME_PARSER_TRACE
  printf("Grouping\n");
#endif
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
#ifdef EXTREME_PARSER_TRACE
  printf("End grouping\n");
#endif
}

static uint8_t identifierConstant(Token *name) {
  return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static uint8_t argumentList() {
  uint8_t numArgs = 0;

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      if (numArgs == 255) {
        error("Can't have more than 255 arguments.");
      }
      numArgs++;
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expected ')' after argument list");
  return numArgs;
}

// OP_CALL just single arg which is the number of arguments.
static void call(bool canAssign) {
  uint8_t numArguments = argumentList();
  emitBytes(OP_CALL, numArguments);
}

static bool identifiersEqual(Token *a, Token *b) {
  // compare length first
  if (a->length != b->length)
    return false;

  return memcmp(a->start, b->start, a->length) == 0;
}

static void addLocal(Token name) {
  if (current->localCount >= UINT8_COUNT) {
    error("Too many local variables in function.");
    return;
  }

  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
  local->isCaptured = false;
}

static void declareVariable() {
  if (current->scopeDepth == 0)
    return;
  Token *name = &parser.previous;

  // Check for existing variables with this name in the current scope by moving
  // backwards in current->locals until one is found or we end up in a higher
  // scope.
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local *local = &current->locals[i];
    // break condition ()
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }

    // found condition
    if (identifiersEqual(&local->name, name)) {
      error("Already a variable with this name in the current scope.");
    }
  }

  addLocal(*name);
}

static int resolveLocal(Compiler *current, Token *name) {
  // All locals in current are in scope, so go in reverse until we find the name
  // that matches and return the index.
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local *local = &current->locals[i];
    // Book assigns to Local* local here but maybe we can just reference
    // directly.
    if (identifiersEqual(&local->name, name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer");
      }
      return i;
    }
  }

  return -1;
}

/**
 * Update a function's upvalue count and set the index and whether or not it is
 * local.
 */
static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal) {
  int upvalueCount = compiler->function->upvalueCount;

  for (int i = 0; i < upvalueCount; i++) {
    Upvalue *upvalue = &compiler->upvalues[i];

    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }

  if (upvalueCount == UINT8_COUNT) {
    error("Too many closure variables in function.");
    return 0;
  }

  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;

  return compiler->function->upvalueCount++;
}

/**
 * This is called after we try to resolve a local value and it isn't in local
 * scope, so we know it's not in the current local scope.
 */
static int resolveUpvalue(Compiler *compiler, Token *name) {
  if (compiler->enclosing == NULL)
    return -1;

  // Does this name exist in the enclosing scope?
  int local = resolveLocal(compiler->enclosing, name);

  if (local != -1) {
    // When we capture a variable from a closure, mark it captured.
    compiler->enclosing->locals[local].isCaptured = true;
    return addUpvalue(compiler, (uint8_t)local, true);
  }

  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(compiler, (uint8_t)upvalue, false);
  }

  return -1;
}

static void namedVariable(Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);

  // -1 is global and should be replaced by the name, not the index
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitBytes(setOp, (uint8_t)arg);
  } else {
    emitBytes(getOp, (uint8_t)arg);
  }
}

static void variable(bool canAssign) {
  // Need to parse the name, emit OP_GET_GLOBAL
  namedVariable(parser.previous, canAssign);
}

// Evaluate left value, if false jump
static void and_(bool canAssign) {
  // we've already parsed the first expression and the and keyword

  // jmp if not false
  int endJump = emitJump(OP_JUMP_IF_FALSE);

  emitByte(OP_POP);
  parsePrecedence(PREC_AND);
  patchJump(endJump);
}

static void or_(bool canAssign) {
  // we've already parsed the first expression and the and keyword

  // true || false, if the value is true jump to end
  // if the value is false, discard the value and move on to the next value?

  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  int endJump = emitJump(OP_JUMP);

  patchJump(elseJump);
  emitByte(OP_POP);

  parsePrecedence(PREC_OR);
  patchJump(endJump);
}

static void literal(bool canAssign) {
  switch (parser.previous.type) {
  case TOKEN_NIL:
    emitByte(OP_NIL);
    break;
  case TOKEN_FALSE:
    emitByte(OP_FALSE);
    break;
  case TOKEN_TRUE:
    emitByte(OP_TRUE);
    break;
  default:
    return;
  }
}

static void string(bool canAssign) {
  // previous token is a whole string token.
  // token has a start and a length, which includes leading and trailing '"'

  Value str = OBJ_VAL(
      copyString(parser.previous.start + 1, parser.previous.length - 2));

  emitConstant(str);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *getRule(TokenType type) { return &rules[type]; }

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

static void printStatement() {
  // Matched print token already
  // compile expression
  expression();
  consume(TOKEN_SEMICOLON, "Expected semicolon after print statement.");

  emitByte(OP_PRINT);
}

static void ifStatement() {
  // parsed if keyword, need to parse
  // left paren
  // condition expression
  // right paren
  // block/statement
  consume(TOKEN_LEFT_PAREN, "Expected '(' after 'if'");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after if predicate");

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();

  // Always jump for else cases if we're in the true case.
  int elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP);
  if (match(TOKEN_ELSE))
    statement();

  patchJump(elseJump);
}

static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  int offset = currentChunk()->count - loopStart + 2;

  if (offset > UINT16_MAX) {
    error("Loop body too large.");
  }

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}

static void whileStatement() {
  int loopStart = currentChunk()->count;
  consume(TOKEN_LEFT_PAREN, "Expected '(' after 'while'");

  expression();

  consume(TOKEN_RIGHT_PAREN, "Expected '(' after 'while'");

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);

  statement();
  emitLoop(loopStart);

  patchJump(exitJump);
  emitByte(OP_POP);
}

static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON)
      return;

    switch (parser.current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;
    default:;
    }

    advance();
  }
}

static uint8_t parseVariable(const char *errorMsg) {
  consume(TOKEN_IDENTIFIER, errorMsg);

  declareVariable();
  // Local variables identifiers aren't stored on the stack.
  if (current->scopeDepth > 0) {
    return 0;
  }

  return identifierConstant(&parser.previous);
}

static void markInitialized() {
  if (current->scopeDepth == 0)
    return;
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }

  emitBytes(OP_DEFINE_GLOBAL, global);
}

static void function(FunctionType type) {
  // Setup new compiler for callstack/scope etc.
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope();

  consume(TOKEN_LEFT_PAREN, "Expected '(' after function name");

  if (!check(TOKEN_RIGHT_PAREN)) {
    // parse arguments.
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("Can't have more than 255 parameters.");
      }

      uint8_t constant = parseVariable("Expected parameter name.");
      defineVariable(constant);
      // there's a comma next.
    } while (match(TOKEN_COMMA));
  }
  // No params for now
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after function parameters.");
  consume(TOKEN_LEFT_BRACE, "Expected '}' after function parameters.");
  block();

  ObjFunction *func = endCompiler();

  // OP_CLOSURE has variable encoding, meaning it could have an arbitrary number
  // of operands.
  emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(func)));

  for (int i = 0; i < func->upvalueCount; i++) {
    // is it local and what's the index
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void varDeclaration() {
  uint8_t global = parseVariable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }

  consume(TOKEN_SEMICOLON, "Expected semicolon after var declaration.");
  defineVariable(global);
}

// fun <name>(args) { body }
static void funDeclaration() {
  uint8_t global = parseVariable("Expect variable name.");
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
}

static void declaration() {
  if (match(TOKEN_VAR)) {
    varDeclaration();
  } else if (match(TOKEN_FUN)) {
    funDeclaration();
  } else {
    statement();
  }

  if (parser.panicMode)
    synchronize();
}

static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expected semicolon after expression.");
  emitByte(OP_POP);
}

/*
 * Instruction layout
 *
 * <expression bytecode> for optional initializer expression
 * loopStart
 * <condition bytecode>
 * JUMP_IF_FALSE <exit offset>
 * OP_POP (conditional no longer needed)
 * <loop body>
 * <increment bytecode> for optional initializer expression
 * OP_LOOP back to loopStart
 * OP_POP @ exit offset
 *
 */
static void forStatement() {
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expected '(' after 'while'");
  // initializer
  // Check if semicolon, otherwise parse a definition? But it could also be a
  // statement

  if (match(TOKEN_SEMICOLON)) {
    // No intiailizer
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    expressionStatement();
  }

  int loopStart = currentChunk()->count;
  int possibleExitJump = -1;

  if (!match(TOKEN_SEMICOLON)) {
    // condition, place in bytecode
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after conditional");

    possibleExitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
  }

  // If you just overwrite loopStart when there's an increment, it makes it
  // simpler. optional increment expression
  if (!match(TOKEN_RIGHT_PAREN)) {
    // Awkward because it shows up before the body but runs after it.
    // Handling this via fun jump magic.

    // start with an always jump to a later offset
    // compile body
    // end

    // jump to body first
    int bodyJump = emitJump(OP_JUMP);
    // compile increment expression
    int incrementStart = currentChunk()->count;
    expression();
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after clauses");
    emitLoop(loopStart);

    loopStart = incrementStart;
    patchJump(bodyJump);
  }

  statement();
  emitLoop(loopStart);

  if (possibleExitJump != -1) {
    patchJump(possibleExitJump);
    // Pop the conditional value
    emitByte(OP_POP);
  }
  endScope();
}

static void returnStatement() {
  if (current->type == TYPE_SCRIPT) {
    error("Can't return from top-level code.");
  }

  if (match(TOKEN_SEMICOLON)) {
    emitReturn();
  } else {
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after return expression.");
    emitByte(OP_RETURN);
  }
}

static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_RETURN)) {
    returnStatement();
  } else if (match(TOKEN_FOR)) {
    forStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else {
    expressionStatement();
  }
}

ObjFunction *compile(const char *source) {
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);

  parser.hadError = false;
  parser.panicMode = false;

  advance();

  while (!match(TOKEN_EOF)) {
    declaration();
  }

  ObjFunction *func = endCompiler();
  if (parser.hadError) {
    return NULL;
  }
  return func;
}

void markCompilerRoots() {
  Compiler *compiler = current;

  while (compiler != NULL) {
    markObject((Obj *)compiler->function);
    compiler = compiler->enclosing;
  }
}
