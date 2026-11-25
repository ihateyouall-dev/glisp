/**
 * @file bignum.c
 * @brief Arbitrary-precision integer arithmetic over base-10^9 limbs.
 */

#include "bignum.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gl_bignum_t *__gl_bignum_alloc(size_t size) {
    gl_bignum_t *res = malloc(sizeof(gl_bignum_t));
    assert(res);
    res->sign = 0;
    res->size = size > 0 ? size : 1;
    res->limbs = calloc(res->size, sizeof(uint32_t));
    assert(res->limbs);
    return res;
}

static void __gl_bignum_normalize(gl_bignum_t *num) {
    while (num->size > 1 && num->limbs[num->size - 1] == 0) {
        --num->size;
    }
    if (num->size == 1 && num->limbs[0] == 0) {
        num->sign = 0;
    }
}

static gl_bignum_t *__gl_bignum_zero(void) {
    gl_bignum_t *zero = __gl_bignum_alloc(1);
    zero->sign = 0;
    return zero;
}

gl_bignum_t *gl_bignum_from_int(int64_t num) {
    uint64_t mag;

    if (num < 0) {
        mag = (uint64_t)(-(num + 1)) + 1;
    } else {
        mag = (uint64_t)num;
    }

    gl_bignum_t *res = __gl_bignum_alloc(3);
    res->size = 0;
    while (mag > 0) {
        res->limbs[res->size++] = (uint32_t)(mag % GL_BIGNUM_BASE);
        mag /= GL_BIGNUM_BASE;
    }
    if (res->size == 0) {
        res->size = 1;
    }
    res->sign = num > 0 ? 1 : (num < 0 ? -1 : 0);
    __gl_bignum_normalize(res);
    return res;
}

void gl_bignum_free(gl_bignum_t *num) {
    if (num == NULL) {
        return;
    }
    free(num->limbs);
    free(num);
}

gl_bignum_t *gl_bignum_copy(const gl_bignum_t *num) {
    if (num == NULL) {
        return NULL;
    }
    gl_bignum_t *res = __gl_bignum_alloc(num->size);
    res->sign = num->sign;
    res->size = num->size;
    memcpy(res->limbs, num->limbs, num->size * sizeof(uint32_t));
    return res;
}

static void __gl_bignum_shift_add(gl_bignum_t *num, uint32_t add, uint32_t mul) {
    uint64_t carry = add;

    for (size_t i = 0; i < num->size; ++i) {
        uint64_t cur = (uint64_t)num->limbs[i] * mul + carry;
        num->limbs[i] = (uint32_t)(cur % GL_BIGNUM_BASE);
        carry = cur / GL_BIGNUM_BASE;
    }
    assert(carry == 0);
}

gl_bignum_t *gl_bignum_from_string(const char *digits) {
    if (digits == NULL || *digits == '\0') {
        return NULL;
    }

    size_t pos = 0;
    int sign = 1;
    if (digits[pos] == '-' || digits[pos] == '+') {
        sign = digits[pos] == '-' ? -1 : 1;
        ++pos;
    }
    if (digits[pos] == '\0') {
        return NULL;
    }

    size_t len = strlen(digits + pos);
    for (size_t i = 0; i < len; ++i) {
        if (!isdigit((unsigned char)digits[pos + i])) {
            return NULL;
        }
    }

    size_t chunk = len % GL_BIGNUM_LIMB_DIGITS;
    if (chunk == 0) {
        chunk = GL_BIGNUM_LIMB_DIGITS;
    }
    size_t limbs = (len + GL_BIGNUM_LIMB_DIGITS - 1) / GL_BIGNUM_LIMB_DIGITS;
    gl_bignum_t *res = __gl_bignum_alloc(limbs);

    size_t offset = 0;
    while (offset < len) {
        uint32_t value = 0;
        for (size_t i = 0; i < chunk; ++i) {
            value = value * 10 + (uint32_t)(digits[pos + offset + i] - '0');
        }
        uint32_t mul = 1;
        for (size_t i = 0; i < chunk; ++i) {
            mul *= 10;
        }
        __gl_bignum_shift_add(res, value, mul);
        offset += chunk;
        chunk = GL_BIGNUM_LIMB_DIGITS;
    }

    res->sign = sign;
    __gl_bignum_normalize(res);
    return res;
}

char *gl_bignum_to_string(const gl_bignum_t *num) {
    if (num == NULL) {
        return NULL;
    }

    size_t cap = num->size * GL_BIGNUM_LIMB_DIGITS + 3;
    char *buf = malloc(cap);
    assert(buf);

    size_t pos = 0;
    if (num->sign < 0) {
        buf[pos++] = '-';
    }
    pos += (size_t)snprintf(buf + pos, cap - pos, "%u", (unsigned)num->limbs[num->size - 1]);
    for (size_t i = num->size - 1; i-- > 0;) {
        pos += (size_t)snprintf(buf + pos, cap - pos, "%09u", (unsigned)num->limbs[i]);
    }
    buf[pos] = '\0';
    return buf;
}

static int __gl_bignum_compare_mag(const gl_bignum_t *lhs, const gl_bignum_t *rhs) {
    if (lhs->size != rhs->size) {
        return lhs->size > rhs->size ? 1 : -1;
    }
    for (size_t i = lhs->size; i-- > 0;) {
        if (lhs->limbs[i] != rhs->limbs[i]) {
            return lhs->limbs[i] > rhs->limbs[i] ? 1 : -1;
        }
    }
    return 0;
}

int gl_bignum_compare(const gl_bignum_t *lhs, const gl_bignum_t *rhs) {
    if (lhs->sign != rhs->sign) {
        return lhs->sign > rhs->sign ? 1 : -1;
    }
    if (lhs->sign == 0) {
        return 0;
    }
    int cmp = __gl_bignum_compare_mag(lhs, rhs);
    return lhs->sign > 0 ? cmp : -cmp;
}

int gl_bignum_compare_int(const gl_bignum_t *lhs, int64_t rhs) {
    uint32_t limbs[3] = {0, 0, 0};
    gl_bignum_t tmp = {0, 1, limbs};
    uint64_t mag = rhs < 0 ? (uint64_t)(-(rhs + 1)) + 1 : (uint64_t)rhs;

    tmp.size = 0;
    while (mag > 0) {
        limbs[tmp.size++] = (uint32_t)(mag % GL_BIGNUM_BASE);
        mag /= GL_BIGNUM_BASE;
    }
    if (tmp.size == 0) {
        tmp.size = 1;
    }
    tmp.sign = rhs > 0 ? 1 : (rhs < 0 ? -1 : 0);
    return gl_bignum_compare(lhs, &tmp);
}

int gl_bignum_fits_int64(const gl_bignum_t *num, int64_t *out) {
    if (num->sign == 0) {
        *out = 0;
        return 1;
    }
    if (num->size > 3) {
        return 0;
    }
    if (num->size == 3 && num->limbs[2] > 9) {
        return 0;
    }

    uint64_t mag = 0;
    for (size_t i = num->size; i-- > 0;) {
        mag = mag * GL_BIGNUM_BASE + num->limbs[i];
    }

    uint64_t limit = (uint64_t)INT64_MAX + 1;
    if (mag > limit) {
        return 0;
    }
    if (mag == limit) {
        if (num->sign > 0) {
            return 0;
        }
        *out = INT64_MIN;
        return 1;
    }
    *out = num->sign < 0 ? -(int64_t)mag : (int64_t)mag;
    return 1;
}

long double gl_bignum_to_float(const gl_bignum_t *num) {
    long double res = 0.0L;
    for (size_t i = num->size; i-- > 0;) {
        res = res * (long double)GL_BIGNUM_BASE + (long double)num->limbs[i];
    }
    return num->sign < 0 ? -res : res;
}

static gl_bignum_t *__gl_bignum_mag_add(const gl_bignum_t *lhs, const gl_bignum_t *rhs) {
    size_t size = (lhs->size > rhs->size ? lhs->size : rhs->size) + 1;
    gl_bignum_t *res = __gl_bignum_alloc(size);
    uint64_t carry = 0;

    for (size_t i = 0; i < size; ++i) {
        uint64_t sum = carry;
        if (i < lhs->size) {
            sum += lhs->limbs[i];
        }
        if (i < rhs->size) {
            sum += rhs->limbs[i];
        }
        res->limbs[i] = (uint32_t)(sum % GL_BIGNUM_BASE);
        carry = sum / GL_BIGNUM_BASE;
    }
    __gl_bignum_normalize(res);
    return res;
}

static gl_bignum_t *__gl_bignum_mag_sub(const gl_bignum_t *lhs, const gl_bignum_t *rhs) {
    gl_bignum_t *res = __gl_bignum_alloc(lhs->size);
    int64_t borrow = 0;

    for (size_t i = 0; i < lhs->size; ++i) {
        int64_t cur =
            (int64_t)lhs->limbs[i] - borrow - (i < rhs->size ? (int64_t)rhs->limbs[i] : 0);
        if (cur < 0) {
            cur += (int64_t)GL_BIGNUM_BASE;
            borrow = 1;
        } else {
            borrow = 0;
        }
        res->limbs[i] = (uint32_t)cur;
    }
    __gl_bignum_normalize(res);
    return res;
}

gl_bignum_t *gl_bignum_neg(const gl_bignum_t *num) {
    gl_bignum_t *res = gl_bignum_copy(num);
    res->sign = -res->sign;
    __gl_bignum_normalize(res);
    return res;
}

gl_bignum_t *gl_bignum_add(const gl_bignum_t *lhs, const gl_bignum_t *rhs) {
    if (lhs->sign == 0) {
        return gl_bignum_copy(rhs);
    }
    if (rhs->sign == 0) {
        return gl_bignum_copy(lhs);
    }

    gl_bignum_t *res;
    if (lhs->sign == rhs->sign) {
        res = __gl_bignum_mag_add(lhs, rhs);
        res->sign = lhs->sign;
        return res;
    }

    int cmp = __gl_bignum_compare_mag(lhs, rhs);
    if (cmp == 0) {
        return __gl_bignum_zero();
    }
    if (cmp > 0) {
        res = __gl_bignum_mag_sub(lhs, rhs);
        res->sign = lhs->sign;
    } else {
        res = __gl_bignum_mag_sub(rhs, lhs);
        res->sign = rhs->sign;
    }
    __gl_bignum_normalize(res);
    return res;
}

gl_bignum_t *gl_bignum_sub(const gl_bignum_t *lhs, const gl_bignum_t *rhs) {
    gl_bignum_t *neg = gl_bignum_neg(rhs);
    gl_bignum_t *res = gl_bignum_add(lhs, neg);
    gl_bignum_free(neg);
    return res;
}

gl_bignum_t *gl_bignum_mul(const gl_bignum_t *lhs, const gl_bignum_t *rhs) {
    gl_bignum_t *res = __gl_bignum_alloc(lhs->size + rhs->size);

    for (size_t i = 0; i < lhs->size; ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < rhs->size; ++j) {
            uint64_t cur = (uint64_t)lhs->limbs[i] * rhs->limbs[j] + res->limbs[i + j] + carry;
            res->limbs[i + j] = (uint32_t)(cur % GL_BIGNUM_BASE);
            carry = cur / GL_BIGNUM_BASE;
        }
        res->limbs[i + rhs->size] = (uint32_t)carry;
    }
    res->sign = lhs->sign * rhs->sign;
    __gl_bignum_normalize(res);
    return res;
}

static gl_bignum_t *__gl_bignum_mag_mul_small(const gl_bignum_t *num, uint32_t factor) {
    gl_bignum_t *res = __gl_bignum_alloc(num->size + 1);
    uint64_t carry = 0;

    for (size_t i = 0; i < num->size; ++i) {
        uint64_t cur = (uint64_t)num->limbs[i] * factor + carry;
        res->limbs[i] = (uint32_t)(cur % GL_BIGNUM_BASE);
        carry = cur / GL_BIGNUM_BASE;
    }
    res->limbs[num->size] = (uint32_t)carry;
    res->size = num->size + 1;
    res->sign = 1;
    __gl_bignum_normalize(res);
    return res;
}

static uint32_t __gl_bignum_mag_div_small(const uint32_t *limbs, size_t size, uint32_t divisor,
                                          uint32_t *out, size_t *out_size) {
    uint64_t rem = 0;
    for (size_t i = size; i-- > 0;) {
        uint64_t cur = rem * GL_BIGNUM_BASE + limbs[i];
        out[i] = (uint32_t)(cur / divisor);
        rem = cur % divisor;
    }
    size_t n = size;
    while (n > 1 && out[n - 1] == 0) {
        --n;
    }
    *out_size = n;
    return (uint32_t)rem;
}

static gl_bignum_t *__gl_bignum_from_limbs(const uint32_t *limbs, size_t size, int sign) {
    gl_bignum_t *res = __gl_bignum_alloc(size);
    memcpy(res->limbs, limbs, size * sizeof(uint32_t));
    res->size = size;
    res->sign = sign;
    __gl_bignum_normalize(res);
    return res;
}

static gl_bignum_t *__gl_bignum_div_knuth(const gl_bignum_t *lhs, const gl_bignum_t *rhs,
                                          gl_bignum_t **rem_out) {
    uint32_t d = (uint32_t)(GL_BIGNUM_BASE / ((uint64_t)rhs->limbs[rhs->size - 1] + 1));
    gl_bignum_t *u = __gl_bignum_mag_mul_small(lhs, d);
    gl_bignum_t *v = __gl_bignum_mag_mul_small(rhs, d);

    uint32_t *grown = realloc(u->limbs, (u->size + 1) * sizeof(uint32_t));
    assert(grown);
    u->limbs = grown;
    u->limbs[u->size] = 0;
    ++u->size;

    size_t n = v->size;
    assert(n >= 2);
    uint32_t vtop = v->limbs[n - 1];
    assert(vtop != 0);
    size_t m = u->size - n;
    uint32_t *quot = malloc(m * sizeof(uint32_t));
    assert(quot);

    for (size_t j = m; j-- > 0;) {
        uint64_t numer = (uint64_t)u->limbs[j + n] * GL_BIGNUM_BASE + u->limbs[j + n - 1];
        uint64_t qhat = numer / vtop;
        uint64_t rhat = numer % vtop;
        while (qhat >= GL_BIGNUM_BASE ||
               qhat * v->limbs[n - 2] > rhat * GL_BIGNUM_BASE + u->limbs[j + n - 2]) {
            --qhat;
            rhat += vtop;
            if (rhat >= GL_BIGNUM_BASE) {
                break;
            }
        }

        uint64_t carry = 0;
        int64_t borrow = 0;
        for (size_t i = 0; i < n; ++i) {
            uint64_t p = qhat * v->limbs[i] + carry;
            carry = p / GL_BIGNUM_BASE;
            int64_t diff = (int64_t)u->limbs[i + j] - (int64_t)(p % GL_BIGNUM_BASE) - borrow;
            if (diff < 0) {
                diff += (int64_t)GL_BIGNUM_BASE;
                borrow = 1;
            } else {
                borrow = 0;
            }
            u->limbs[i + j] = (uint32_t)diff;
        }

        int64_t top = (int64_t)u->limbs[j + n] - (int64_t)carry - borrow;
        if (top < 0) {
            --qhat;
            uint64_t add_carry = 0;
            for (size_t i = 0; i < n; ++i) {
                uint64_t sum = (uint64_t)u->limbs[i + j] + v->limbs[i] + add_carry;
                u->limbs[i + j] = (uint32_t)(sum % GL_BIGNUM_BASE);
                add_carry = sum / GL_BIGNUM_BASE;
            }
            top += (int64_t)add_carry;
        }
        assert(top >= 0 && top < (int64_t)GL_BIGNUM_BASE);
        u->limbs[j + n] = (uint32_t)top;
        quot[j] = (uint32_t)qhat;
    }

    gl_bignum_t *q = __gl_bignum_from_limbs(quot, m, lhs->sign * rhs->sign);
    free(quot);

    gl_bignum_t *r = __gl_bignum_from_limbs(u->limbs, n, lhs->sign);
    if (d != 1) {
        uint32_t *tmp = malloc((r->size > 0 ? r->size : 1) * sizeof(uint32_t));
        assert(tmp);
        size_t rsize = 0;
        __gl_bignum_mag_div_small(r->limbs, r->size, d, tmp, &rsize);
        r->size = rsize;
        memcpy(r->limbs, tmp, rsize * sizeof(uint32_t));
        free(tmp);
    }
    __gl_bignum_normalize(r);
    gl_bignum_free(u);
    gl_bignum_free(v);
    *rem_out = r;
    return q;
}

gl_bignum_t *gl_bignum_div(const gl_bignum_t *lhs, const gl_bignum_t *rhs, gl_bignum_t **rem) {
    if (rhs->sign == 0) {
        return NULL;
    }

    int cmp = __gl_bignum_compare_mag(lhs, rhs);
    if (cmp < 0) {
        *rem = gl_bignum_copy(lhs);
        return __gl_bignum_zero();
    }
    if (rhs->size == 1) {
        uint32_t *quot = malloc(lhs->size * sizeof(uint32_t));
        assert(quot);
        size_t qsize = 0;
        uint32_t rem_digit =
            __gl_bignum_mag_div_small(lhs->limbs, lhs->size, rhs->limbs[0], quot, &qsize);
        gl_bignum_t *q = __gl_bignum_from_limbs(quot, qsize, lhs->sign * rhs->sign);
        free(quot);
        uint32_t rem_limb = rem_digit;
        gl_bignum_t *r = __gl_bignum_from_limbs(&rem_limb, 1, lhs->sign);
        *rem = r;
        return q;
    }
    return __gl_bignum_div_knuth(lhs, rhs, rem);
}
