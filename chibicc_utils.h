// Utilities of chibicc
#ifndef CHIBICC_UTILS_H_
#define CHIBICC_UTILS_H_

#include "chibicc_error.h"

/**
 * @brief Aligns a value up to the next specified alignment boundary.
 * @note The alignment value must be a power of two (e.g. 2, 4, 8, 16, ...).
 *
 * @param n The size to be aligned.
 * @param align The alignment boundary in bytes.
 *
 * @return The aligned size, rounded up to the nearest multiple of 'align'.
 */
static inline int align_to(int n, int align) {
  CHECK(align > 0 || (align & (align - 1)) == 0);
  return (n + align - 1) & ~(align - 1);
}

#endif // CHIBICC_UTILS_H_
