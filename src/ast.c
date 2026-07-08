#include "ast.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// MSVC has _strdup instead of strdup
#ifdef _MSC_VER
#define strdup _strdup
#endif

ASTNode *ast_make_int(int64_t num) {
    ASTNode *res = malloc(sizeof(ASTNode));
    assert(res);
    res->type = AST_INT;
    res->value.integral = num;
    return res;
}

ASTNode *ast_make_float(long double num) {
    ASTNode *res = malloc(sizeof(ASTNode));
    assert(res);
    res->type = AST_FLOAT;
    res->value.floating = num;
    return res;
}

ASTNode *ast_make_symbol(const char *sym) {
    ASTNode *res = malloc(sizeof(ASTNode));
    assert(res);
    res->type = AST_SYMBOL;
    res->value.symbol = strdup(sym);
    return res;
}

ASTNode *ast_make_cons(ASTNode *car, ASTNode *cdr) {
    ASTNode *res = malloc(sizeof(ASTNode));
    assert(res);
    res->type = AST_CONS;
    res->value.cons.car = car;
    res->value.cons.cdr = cdr;
    return res;
}

ASTNode *ast_make_nil(void) {
    ASTNode *res = malloc(sizeof(ASTNode));
    assert(res);
    res->type = AST_NIL;
    return res;
}

void ast_destroy(ASTNode *node) {
    if (!node) {
        return;
    }

    switch (node->type) {
    case AST_CONS:
        ast_destroy(node->value.cons.car);
        ast_destroy(node->value.cons.cdr);
        break;
    case AST_SYMBOL:
        free(node->value.symbol);
        break;
    default:
        break;
    }

    free(node);
}
