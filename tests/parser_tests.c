#include "ast.h"
#include "parser.h"
#include "test.h"

#include <string.h>

int test_status = 0;

void test_parse_int(void) {
    const char *src = "123";
    gl_parser_t parser = gl_make_parser(src);
    gl_ast_node_t *node = gl_parser_parse(&parser);
    TEST(node->type == GL_AST_INT, "Parse INT");
    TEST(node->value.integral == 123, "Parse INT value");
    gl_parser_destroy(&parser);
}

void test_parse_float(void) {
    const char *src = "45.67";
    gl_parser_t parser = gl_make_parser(src);
    gl_ast_node_t *node = gl_parser_parse(&parser);
    TEST(node->type == GL_AST_FLOAT, "Parse FLOAT");
    TEST(node->value.floating == 45.67, "Parse FLOAT value");
    gl_parser_destroy(&parser);
}

void test_parse_symbol(void) {
    const char *src = "my_symbol";
    gl_parser_t parser = gl_make_parser(src);
    gl_ast_node_t *node = gl_parser_parse(&parser);
    TEST(node->type == GL_AST_SYMBOL, "Parse SYMBOL");
    TEST(strcmp(node->value.symbol, "my_symbol") == 0, "Parse SYMBOL value");
    gl_parser_destroy(&parser);
}

void test_parse_simple_list(void) {
    const char *src = "(1 2 3)";
    gl_parser_t parser = gl_make_parser(src);
    gl_ast_node_t *list = gl_parser_parse(&parser);
    TEST(list->type == GL_AST_CONS, "Parse simple list type");
    TEST(list->value.cons.car->type == GL_AST_INT, "List car 1");
    TEST(list->value.cons.car->value.integral == 1, "List car 1 value");
    TEST(list->value.cons.cdr->type == GL_AST_CONS, "List cdr type");
    TEST(list->value.cons.cdr->value.cons.car->type == GL_AST_INT, "List car 2");
    TEST(list->value.cons.cdr->value.cons.car->value.integral == 2, "List car 2 value");
    gl_parser_destroy(&parser);
}

void test_parse_nested_list(void) {
    const char *src = "(1 (2 3) 4)";
    gl_parser_t parser = gl_make_parser(src);
    gl_ast_node_t *list = gl_parser_parse(&parser);
    TEST(list->type == GL_AST_CONS, "Parse nested list type");
    TEST(list->value.cons.car->type == GL_AST_INT, "Nested list car 1");
    TEST(list->value.cons.car->value.integral == 1, "Nested list car 1 value");
    TEST(list->value.cons.cdr->type == GL_AST_CONS, "Nested list cdr type");

    gl_ast_node_t *inner = list->value.cons.cdr;
    TEST(inner->value.cons.car->type == GL_AST_CONS, "Nested inner car type");
    TEST(inner->value.cons.car->value.cons.car->type == GL_AST_INT, "Nested inner car 2");
    TEST(inner->value.cons.car->value.cons.car->value.integral == 2, "Nested inner car 2 value");
    TEST(inner->value.cons.car->value.cons.cdr->type == GL_AST_CONS, "Nested inner car 2 cdr type");
    TEST(inner->value.cons.car->value.cons.cdr->value.cons.car->type == GL_AST_INT,
         "Nested inner car 2 cdr car 3");
    TEST(inner->value.cons.car->value.cons.cdr->value.cons.car->value.integral == 3,
         "Nested inner car 2 cdr car 3 value");
    TEST(inner->value.cons.car->value.cons.cdr->value.cons.cdr->type == GL_AST_NIL,
         "Nested inner car 2 cdr cdr NIL");
    TEST(inner->value.cons.cdr->type == GL_AST_CONS, "Nested inner cdr type");
    TEST(inner->value.cons.cdr->value.cons.car->type == GL_AST_INT, "Nested inner cdr car 4");
    TEST(inner->value.cons.cdr->value.cons.car->value.integral == 4,
         "Nested inner cdr car 4 value");
    TEST(inner->value.cons.cdr->value.cons.cdr->type == GL_AST_NIL,
         "Nested inner cdr car 4 cdr NIL");
    gl_parser_destroy(&parser);
}

void test_parse_complex_list(void) {
    const char *src = "((a b) c)";
    gl_parser_t parser = gl_make_parser(src);
    gl_ast_node_t *list = gl_parser_parse(&parser);
    TEST(list->type == GL_AST_CONS, "Parse complex list type");
    TEST(list->value.cons.car->type == GL_AST_CONS, "Complex list car type");
    TEST(list->value.cons.car->value.cons.car->type == GL_AST_SYMBOL,
         "Complex list car car symbol");
    TEST(strcmp(list->value.cons.car->value.cons.car->value.symbol, "a") == 0,
         "Complex list car car symbol value");
    TEST(list->value.cons.car->value.cons.cdr->type == GL_AST_CONS, "Complex list car car cdr");
    TEST(list->value.cons.car->value.cons.cdr->value.cons.car->type == GL_AST_SYMBOL,
         "Complex list car car cdr car symbol");
    TEST(strcmp(list->value.cons.car->value.cons.cdr->value.cons.car->value.symbol, "b") == 0,
         "Complex list car car cdr car symbol value");
    TEST(list->value.cons.car->value.cons.cdr->value.cons.cdr->type == GL_AST_NIL,
         "Complex list car car cdr cdr NIL");
    TEST(list->value.cons.cdr->type == GL_AST_CONS, "Complex list cdr type");
    TEST(list->value.cons.cdr->value.cons.car->type == GL_AST_SYMBOL,
         "Complex list cdr car symbol");
    TEST(strcmp(list->value.cons.cdr->value.cons.car->value.symbol, "c") == 0,
         "Complex list cdr car symbol value");
    TEST(list->value.cons.cdr->value.cons.cdr->type == GL_AST_NIL, "Complex list cdr car cdr NIL");
    gl_parser_destroy(&parser);
}

void parser_test(void) {
    test_parse_int();
    test_parse_float();
    test_parse_symbol();
    test_parse_simple_list();
    test_parse_nested_list();
    test_parse_complex_list();
}

int main(void) {
    parser_test();
    return test_status;
}
