#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * ANSI Escape Codes
 */
#define GL_ANSI_RESET "\x1b[0m"

#define GL_ANSI_BOLD "\x1b[1m"
#define GL_ANSI_DIM "\x1b[2m"
#define GL_ANSI_ITALIC "\x1b[3m"
#define GL_ANSI_UNDERLINE "\x1b[4m"
#define GL_ANSI_BLINK "\x1b[5m"
#define GL_ANSI_REVERSE "\x1b[7m"
#define GL_ANSI_HIDDEN "\x1b[8m"
#define GL_ANSI_STRIKETHROUGH "\x1b[9m"

// Standard foreground colors
#define GL_ANSI_FG_BLACK "\x1b[30m"
#define GL_ANSI_FG_RED "\x1b[31m"
#define GL_ANSI_FG_GREEN "\x1b[32m"
#define GL_ANSI_FG_YELLOW "\x1b[33m"
#define GL_ANSI_FG_BLUE "\x1b[34m"
#define GL_ANSI_FG_MAGENTA "\x1b[35m"
#define GL_ANSI_FG_CYAN "\x1b[36m"
#define GL_ANSI_FG_WHITE "\x1b[37m"

// Bright foreground colors
#define GL_ANSI_FG_BRIGHT_BLACK "\x1b[90m"
#define GL_ANSI_FG_BRIGHT_RED "\x1b[91m"
#define GL_ANSI_FG_BRIGHT_GREEN "\x1b[92m"
#define GL_ANSI_FG_BRIGHT_YELLOW "\x1b[93m"
#define GL_ANSI_FG_BRIGHT_BLUE "\x1b[94m"
#define GL_ANSI_FG_BRIGHT_MAGENTA "\x1b[95m"
#define GL_ANSI_FG_BRIGHT_CYAN "\x1b[96m"
#define GL_ANSI_FG_BRIGHT_WHITE "\x1b[97m"

// Standard background colors
#define GL_ANSI_BG_BLACK "\x1b[40m"
#define GL_ANSI_BG_RED "\x1b[41m"
#define GL_ANSI_BG_GREEN "\x1b[42m"
#define GL_ANSI_BG_YELLOW "\x1b[43m"
#define GL_ANSI_BG_BLUE "\x1b[44m"
#define GL_ANSI_BG_MAGENTA "\x1b[45m"
#define GL_ANSI_BG_CYAN "\x1b[46m"
#define GL_ANSI_BG_WHITE "\x1b[47m"

// Bright background colors
#define GL_ANSI_BG_BRIGHT_BLACK "\x1b[100m"
#define GL_ANSI_BG_BRIGHT_RED "\x1b[101m"
#define GL_ANSI_BG_BRIGHT_GREEN "\x1b[102m"
#define GL_ANSI_BG_BRIGHT_YELLOW "\x1b[103m"
#define GL_ANSI_BG_BRIGHT_BLUE "\x1b[104m"
#define GL_ANSI_BG_BRIGHT_MAGENTA "\x1b[105m"
#define GL_ANSI_BG_BRIGHT_CYAN "\x1b[106m"
#define GL_ANSI_BG_BRIGHT_WHITE "\x1b[107m"

static inline int __gl_ansi_hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/**
 * Processes escaped characters in a string and returns a newly allocated string:
 *  - Standard C escapes: \n, \r, \t, \a, \b, \f, \v, \\, \', \", \?
 *  - ANSI escape byte: \e, \E (0x1B)
 *  - Hexadecimal byte: \xHH (e.g. \x1b)
 *  - Octal byte: \0OO / \OOO (e.g. \033)
 *
 * @param str The null-terminated input string (can be a string literal).
 * @return A newly allocated null-terminated string, or NULL on error / if str is NULL.
 *         The caller is responsible for calling free() on the returned pointer.
 */
static inline char *gl_process_ansi_str(const char *str) {
    if (!str) {
        return NULL;
    }

    size_t len = strlen(str);
    char *result = malloc(len + 1);
    if (!result) {
        return NULL;
    }

    const char *src = str;
    char *dst = result;

    while (*src) {
        if (*src == '\\') {
            src++;
            switch (*src) {
            case '\0':
                *dst++ = '\\';
                goto __gl_ansi_end;
            case 'a':
                *dst++ = '\a';
                src++;
                break;
            case 'b':
                *dst++ = '\b';
                src++;
                break;
            case 'f':
                *dst++ = '\f';
                src++;
                break;
            case 'n':
                *dst++ = '\n';
                src++;
                break;
            case 'r':
                *dst++ = '\r';
                src++;
                break;
            case 't':
                *dst++ = '\t';
                src++;
                break;
            case 'v':
                *dst++ = '\v';
                src++;
                break;
            case 'e':
            case 'E':
                *dst++ = '\x1b';
                src++;
                break;
            case '\\':
                *dst++ = '\\';
                src++;
                break;
            case '\'':
                *dst++ = '\'';
                src++;
                break;
            case '"':
                *dst++ = '"';
                src++;
                break;
            case '?':
                *dst++ = '?';
                src++;
                break;
            case 'x':
            case 'X': {
                src++;
                int h1 = __gl_ansi_hex_digit(*src);
                if (h1 >= 0) {
                    src++;
                    int h2 = __gl_ansi_hex_digit(*src);
                    if (h2 >= 0) {
                        *dst++ = (char)((h1 << 4) | h2);
                        src++;
                    } else {
                        *dst++ = (char)h1;
                    }
                } else {
                    *dst++ = '\\';
                    *dst++ = *(src - 1);
                }
                break;
            }
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7': {
                int val = 0;
                int count = 0;
                while (count < 3 && (*src >= '0' && *src <= '7')) {
                    val = (val << 3) + (*src - '0');
                    src++;
                    count++;
                }
                *dst++ = (char)val;
                break;
            }
            default:
                *dst++ = *src;
                src++;
                break;
            }
        } else {
            *dst++ = *src++;
        }
    }

__gl_ansi_end:
    *dst = '\0';
    return result;
}

/**
 * Processes escaped characters in a mutable string in-place:
 *  - Standard C escapes: \n, \r, \t, \a, \b, \f, \v, \\, \', \", \?
 *  - ANSI escape byte: \e, \E (0x1B)
 *  - Hexadecimal byte: \xHH (e.g. \x1b)
 *  - Octal byte: \0OO / \OOO (e.g. \033)
 *
 * @param str The null-terminated mutable string to process in-place.
 * @return The pointer to the modified string `str`, or NULL if `str` is NULL.
 */
static inline char *gl_process_ansi_str_inplace(char *str) {
    if (!str) {
        return NULL;
    }

    char *src = str;
    char *dst = str;

    while (*src) {
        if (*src == '\\') {
            src++;
            switch (*src) {
            case '\0':
                *dst++ = '\\';
                goto __gl_ansi_inplace_end;
            case 'a':
                *dst++ = '\a';
                src++;
                break;
            case 'b':
                *dst++ = '\b';
                src++;
                break;
            case 'f':
                *dst++ = '\f';
                src++;
                break;
            case 'n':
                *dst++ = '\n';
                src++;
                break;
            case 'r':
                *dst++ = '\r';
                src++;
                break;
            case 't':
                *dst++ = '\t';
                src++;
                break;
            case 'v':
                *dst++ = '\v';
                src++;
                break;
            case 'e':
            case 'E':
                *dst++ = '\x1b';
                src++;
                break;
            case '\\':
                *dst++ = '\\';
                src++;
                break;
            case '\'':
                *dst++ = '\'';
                src++;
                break;
            case '"':
                *dst++ = '"';
                src++;
                break;
            case '?':
                *dst++ = '?';
                src++;
                break;
            case 'x':
            case 'X': {
                src++;
                int h1 = __gl_ansi_hex_digit(*src);
                if (h1 >= 0) {
                    src++;
                    int h2 = __gl_ansi_hex_digit(*src);
                    if (h2 >= 0) {
                        *dst++ = (char)((h1 << 4) | h2);
                        src++;
                    } else {
                        *dst++ = (char)h1;
                    }
                } else {
                    *dst++ = '\\';
                    *dst++ = *(src - 1);
                }
                break;
            }
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7': {
                int val = 0;
                int count = 0;
                while (count < 3 && (*src >= '0' && *src <= '7')) {
                    val = (val << 3) + (*src - '0');
                    src++;
                    count++;
                }
                *dst++ = (char)val;
                break;
            }
            default:
                *dst++ = *src;
                src++;
                break;
            }
        } else {
            *dst++ = *src++;
        }
    }

__gl_ansi_inplace_end:
    *dst = '\0';
    return str;
}
