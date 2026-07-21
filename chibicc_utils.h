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
static inline long align_to(long n, long align) {
  CHECK(align > 0 || (align & (align - 1)) == 0);
  return (n + align - 1) & ~(align - 1);
}

// Gets maximum value of signed integer by width
static inline long int_max_by_width(int width) {
  return (1LL << (width - 1)) - 1LL;
}

// Gets minimum value of signed integer by width
static inline long int_min_by_width(int width) { return -(1LL << (width - 1)); }

/**
 * @brief Check a given value is in a range of 12-bit signed integer for
 * immediate instructions.
 */
static inline bool is_valid_riscv_imm12(long val) {
  return (val >= int_min_by_width(12)) && (val <= int_max_by_width(12));
}

#endif // CHIBICC_UTILS_H_
