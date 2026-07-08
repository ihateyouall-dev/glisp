#include "lexer.h"
#include "vector.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void lexer_init(Lexer *lexer, const char *src) {
    lexer->src = src;

    lexer->len = strlen(src);
    lexer->location.pos = 0;
    lexer->location.line = 1;
    lexer->location.column = 1;
}

char lexer_current(Lexer *lexer) { return lexer->src[lexer->location.pos]; }

char lexer_peek(Lexer *lexer) { return lexer->src[lexer->location.pos + 1]; }

void lexer_advance(Lexer *lexer) {
    char ch = lexer->src[++lexer->location.pos];

    if (ch == '\n') {
        ++lexer->location.line;
        lexer->location.column = 0;
    } else {
        ++lexer->location.column;
    }
}

static void __lexer_skip_line(Lexer *lexer) {
    char current;
    while ((current = lexer_current(lexer)) != '\n' && current != '\0') {
        lexer_advance(lexer);
    }

    // Putting lexer right after newline
    if (current == '\n') {
        lexer_advance(lexer);
    }
}

static void __lexer_skip_empty(Lexer *lexer) {
begin:
    while (isspace(lexer_current(lexer))) {
        lexer_advance(lexer);
    }

    // Handling comments
    if (lexer_current(lexer) == ';') {
        __lexer_skip_line(lexer);
        goto begin;
    }
}

static int __issymbol(char ch) {
    return !isspace((unsigned char)ch) && ch != '(' && ch != ')' && ch != ';' && ch != '"' &&
           ch != '\0';
}

static LexToken __lexer_read_token(Lexer *lexer) {
    LexToken res;
    res.type = LEX_UNKNOWN;
    res.status = LEX_OK;
    res.location = lexer->location;

    switch (lexer_current(lexer)) {
    case '(':
        res.type = LEX_LPAREN;
        lexer_advance(lexer);
        return res;
    case ')':
        res.type = LEX_RPAREN;
        lexer_advance(lexer);
        return res;
    case '\'':
        res.type = LEX_QUOTE;
        lexer_advance(lexer);
        return res;
    case '\0':
        res.type = LEX_EOF;
        return res;
    }

    const char *start = lexer->src + lexer->location.pos;
    const size_t start_pos = lexer->location.pos;

    // Detecting possible numeric literal
    if (isdigit(lexer_current(lexer))) {
        res.type = LEX_INTLITERAL;
        while (isdigit(lexer_current(lexer))) {
            lexer_advance(lexer);
        }
        // Detecting float literal
        if (lexer_current(lexer) == '.') {
            res.type = LEX_FLOATLITERAL;
            lexer_advance(lexer);
            // Counting rest of digits after delimiter
            while (isdigit(lexer_current(lexer))) {
                lexer_advance(lexer);
            }
        }
    }
    while (__issymbol(lexer_current(lexer))) {
        res.type = LEX_SYMBOL;
        lexer_advance(lexer);
    }
    if (res.type == LEX_UNKNOWN) {
        res.status = LEX_ERROR;
        lexer_advance(lexer);
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

LexToken lexer_next(Lexer *lexer) {
    __lexer_skip_empty(lexer);
    return __lexer_read_token(lexer);
}

VECTOR_DEFINE(LexToken, LexTokenVector)

LexTokenVector lexer_tokenize(Lexer *lexer) {
    LexTokenVector res;
    LexTokenVector_init(&res);

    LexToken current;

    do {
        current = lexer_next(lexer);
        LexTokenVector_push_back(&res, current);
    } while (current.type != LEX_EOF);

    return res;
}

Lexer make_lexer(const char *src) {
    Lexer lexer;
    lexer_init(&lexer, src);
    return lexer;
}
