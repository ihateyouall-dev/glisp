#pragma once

#include "vector.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    VAL_NIL,
    VAL_BOOLEAN,
    VAL_INT,
    VAL_FLOAT,
    VAL_SYMBOL,
    VAL_FUNCTION,
    VAL_BUILTIN,
    VAL_CONS
} ValueType;

typedef struct {
    ValueType type;
    void *val;
} Value;

VECTOR_DECLARE(Value, ValueVector)

typedef struct {
    ValueVector *values;

} ValueTable;

typedef struct Env {

} Env;
