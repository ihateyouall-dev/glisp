#pragma once

/**
 * @file cons.h
 * @brief Generic cons-cell container macros.
 */

/**
 * Declares a typed cons cell named @p Name whose car and cdr have type @p T.
 *
 * Use CONS_DEFINE(T, Name) in exactly one translation unit to emit the
 * implementation. The generated destroy function frees the cons cell itself;
 * element ownership is the caller's responsibility.
 */
#define CONS_DECLARE(T, Name)                                                                      \
    typedef struct {                                                                               \
        T car;                                                                                     \
        T cdr;                                                                                     \
    } Name##_t;
