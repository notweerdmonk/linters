/*
 * Copyright (C) 2025  notweerdmonk
 *
 * This file is part of gdblint.
 *
 * Gdblint is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gdblint is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gdblint.  If not, see <https://www.gnu.org/licenses/>.
 */
/**
 * @file hashmap.c
 * @brief Unit test for hashmap
 */

#include <src/gdblint.c>
#include <assert.h>

#define TEST_CASE(description, condition, message) \
  do {\
    puts("Test: "description);\
    puts("Condition: "#condition); \
    assert(condition && message);\
    puts("Result: PASS\n");\
  } while(0)

int main() {
  FILE *cachefp = setup_cache(1);

  /* Test setting up the cache */
  TEST_CASE(
      "Setup cache",
      (cachefp = setup_cache(0)) != NULL,
      "Could not setup cache"
    );

  /* Test getting cache FILE* after setup */
  TEST_CASE(
      "Get cache FILE* after setup",
      setup_cache(0) == cachefp,
      "Cache FILE* mismatch"
    );

  /* Test clearing the cache */
  TEST_CASE(
      "Clear the cache and recreate it",
      setup_cache(1) == NULL,
      "Could not clear cache or recreate it"
    );

  return 0;
}
