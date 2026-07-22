#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"


#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) (isObjType(value, OBJ_STRING))

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

struct ObjString {
  Obj obj;
  int length;
  char* chars;
};

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int lenght);
void printObject(Value value);

static inline bool isObjType(Value val, ObjType type) {
// is an object value, is the object pointer have a type of |type|
    if (!IS_OBJ(val)) return false;

    return OBJ_TYPE(val) == type;
}

#endif
