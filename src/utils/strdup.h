#pragma once

/**
 * @file strdup.h
 * @brief Portable strdup shim.
 */

#include <string.h>

// MSVC has _strdup instead of strdup
#ifdef _MSC_VER
#define strdup _strdup
#endif
