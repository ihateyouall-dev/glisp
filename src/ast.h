#pragma once

#include <stdint.h>

typedef enum { AST_INT, AST_FLOAT, AST_SYMBOL, AST_CONS, AST_NIL } ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;

    union {
        int64_t integral;
        long double floating;
        char *symbol;

        struct {
            struct ASTNode *car;
            struct ASTNode *cdr;
        } cons;
    } value;
} ASTNode;

ASTNode *ast_make_int(int64_t num);
ASTNode *ast_make_float(long double num);
ASTNode *ast_make_symbol(const char *sym);
ASTNode *ast_make_cons(ASTNode *car, ASTNode *cdr);
ASTNode *ast_make_nil(void);
void ast_destroy(ASTNode *node);
