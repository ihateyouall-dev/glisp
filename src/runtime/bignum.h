/**
 * @file bignum.h
 * @brief Arbitrary-precision integer payloads in base 10^9.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/** Base of the limb representation. */
#define GL_BIGNUM_BASE 1000000000ULL

/** Number of decimal digits in one limb. */
#define GL_BIGNUM_LIMB_DIGITS 9

/**
 * A sign-magnitude arbitrary-precision integer.
 *
 * Limbs are little-endian and every limb is below GL_BIGNUM_BASE. The most
 * significant limb is never zero unless the value itself is zero, in which
 * case @ref sign is zero as well.
 */
typedef struct {
    int sign;        /**< 1 for positive, -1 for negative, 0 for zero. */
    size_t size;     /**< Number of significant limbs. */
    uint32_t *limbs; /**< Little-endian limbs, each below GL_BIGNUM_BASE. */
} gl_bignum_t;

/** Parses a decimal string with an optional sign. Returns NULL on bad input. */
gl_bignum_t *gl_bignum_from_string(const char *digits);

/** Builds a payload from an int64_t. */
gl_bignum_t *gl_bignum_from_int(int64_t num);

/** Frees a payload and its limbs. */
void gl_bignum_free(gl_bignum_t *num);

/** Returns a deep copy of @p num. */
gl_bignum_t *gl_bignum_copy(const gl_bignum_t *num);

/** Returns a heap-allocated decimal representation of @p num. */
char *gl_bignum_to_string(const gl_bignum_t *num);

/** Compares two payloads and returns -1, 0, or 1. */
int gl_bignum_compare(const gl_bignum_t *lhs, const gl_bignum_t *rhs);

/** Compares a payload with an int64_t and returns -1, 0, or 1. */
int gl_bignum_compare_int(const gl_bignum_t *lhs, int64_t rhs);

/** Writes @p num to @p out when it fits in an int64_t; returns 1 on success. */
int gl_bignum_fits_int64(const gl_bignum_t *num, int64_t *out);

/** Converts @p num to long double, rounding when needed. */
long double gl_bignum_to_float(const gl_bignum_t *num);

/** Adds two payloads. */
gl_bignum_t *gl_bignum_add(const gl_bignum_t *lhs, const gl_bignum_t *rhs);

/** Subtracts @p rhs from @p lhs. */
gl_bignum_t *gl_bignum_sub(const gl_bignum_t *lhs, const gl_bignum_t *rhs);

/** Multiplies two payloads. */
gl_bignum_t *gl_bignum_mul(const gl_bignum_t *lhs, const gl_bignum_t *rhs);

/** Returns the negation of @p num. */
gl_bignum_t *gl_bignum_neg(const gl_bignum_t *num);

/**
 * Truncating division: returns the quotient and stores the remainder, which
 * follows the dividend's sign, in @p rem. Returns NULL when @p rhs is zero.
 */
gl_bignum_t *gl_bignum_div(const gl_bignum_t *lhs, const gl_bignum_t *rhs, gl_bignum_t **rem);
