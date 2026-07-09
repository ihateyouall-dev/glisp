#include "lexer.h"
#include "vector.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void gl_lexer_init(gl_lexer *lexer, const char *src) {
    lexer->src = src;

    lexer->len = strlen(src);
    lexer->location.pos = 0;
    lexer->location.line = 1;
    lexer->location.column = 1;
}

char gl_lexer_current(gl_lexer *lexer) { return lexer->src[lexer->location.pos]; }

char gl_lexer_peek(gl_lexer *lexer) { return lexer->src[lexer->location.pos + 1]; }

void gl_lexer_advance(gl_lexer *lexer) {
    char ch = lexer->src[++lexer->location.pos];

    if (ch == '\n') {
        ++lexer->location.line;
        lexer->location.column = 0;
    } else {
        ++lexer->location.column;
    }
}

static void __lexer_skip_line(gl_lexer *lexer) {
    char current;
    while ((current = gl_lexer_current(lexer)) != '\n' && current != '\0') {
        gl_lexer_advance(lexer);
    }

    // Putting lexer right after newline
    if (current == '\n') {
        gl_lexer_advance(lexer);
    }
}

static void __lexer_skip_empty(gl_lexer *lexer) {
begin:
    while (isspace(gl_lexer_current(lexer))) {
        gl_lexer_advance(lexer);
    }

    // Handling comments
    if (gl_lexer_current(lexer) == ';') {
        __lexer_skip_line(lexer);
        goto begin;
    }
}

static int __issymbol(char ch) {
    return !isspace((unsigned char)ch) && ch != '(' && ch != ')' && ch != ';' && ch != '"' &&
           ch != '\0';
}

static gl_lex_token __lexer_read_token(gl_lexer *lexer) {
    gl_lex_token res;
    res.type = GL_LEX_UNKNOWN;
    res.status = LEX_OK;
    res.location = lexer->location;

    switch (gl_lexer_current(lexer)) {
    case '(':
        res.type = GL_LEX_LPAREN;
        gl_lexer_advance(lexer);
        return res;
    case ')':
        res.type = GL_LEX_RPAREN;
        gl_lexer_advance(lexer);
        return res;
    case '\'':
        res.type = GL_LEX_QUOTE;
        gl_lexer_advance(lexer);
        return res;
    case '\0':
        res.type = GL_LEX_EOF;
        return res;
    }

    const char *start = lexer->src + lexer->location.pos;
    const size_t start_pos = lexer->location.pos;

    // Detecting possible numeric literal
    if (isdigit(gl_lexer_current(lexer))) {
        res.type = GL_LEX_INTLITERAL;
        while (isdigit(gl_lexer_current(lexer))) {
            gl_lexer_advance(lexer);
        }
        // Detecting float literal
        if (gl_lexer_current(lexer) == '.') {
            res.type = GL_LEX_FLOATLITERAL;
            gl_lexer_advance(lexer);
            // Counting rest of digits after delimiter
            while (isdigit(gl_lexer_current(lexer))) {
                gl_lexer_advance(lexer);
            }
        }
    }
    while (__issymbol(gl_lexer_current(lexer))) {
        res.type = GL_LEX_SYMBOL;
        gl_lexer_advance(lexer);
    }
    if (res.type == GL_LEX_UNKNOWN) {
        res.status = LEX_ERROR;
        gl_lexer_advance(lexer);
        return res;
    }

    // Constructing and assigning value string to result
    const size_t current_pos = lexer->location.pos;
    const size_t length = current_pos - start_pos;

    char *buf = malloc(length + 1);

    memcpy(buf, start, length);
    buf[length] = '\0';

    res.value = buf;
    return res;
}

gl_lex_token gl_lexer_next(gl_lexer *lexer) {
    __lexer_skip_empty(lexer);
    return __lexer_read_token(lexer);
}

VECTOR_DEFINE(gl_lex_token, gl_lex_token_vector)

gl_lex_token_vector gl_lexer_tokenize(gl_lexer *lexer) {
    gl_lex_token_vector res;
    gl_lex_token_vector_init(&res);

    gl_lex_token current;

    do {
        current = gl_lexer_next(lexer);
        gl_lex_token_vector_push_back(&res, current);
    } while (current.type != GL_LEX_EOF);

    return res;
}

gl_lexer gl_make_lexer(const char *src) {
    gl_lexer lexer;
    gl_lexer_init(&lexer, src);
    return lexer;
}
