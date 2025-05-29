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
  struct hash_map map;
  init_map(&map);
  insert_symbol(&map, "test_symbol", 1, VAR);

  /* Test insertion of an entry */
  TEST_CASE(
      "Symbol insertion into hash map",
      find_symbol(&map, "test_symbol", VAR) != NULL,
      "Could not insert symbol"
    );

  /* Test search of non-existent entry */
  TEST_CASE(
      "Find non-existent symbol in hash map",
      find_symbol(&map, "not_found", VAR) == NULL,
      "Errneous symbol found"
    );


  /* Test duplicate entry check */
  struct symbol *sym = find_symbol(&map, "test_symbol", VAR);
  insert_symbol(&map, "test_symbol", 1, VAR);

#ifdef CFG_HASHMAP_CHK_DUPLICATES
  TEST_CASE(
      "Check duplicate symbols in hash map",
      find_symbol(&map, "test_symbol", VAR) == sym,
      "Duplicate symbols found"
    );
#else
  TEST_CASE(
      "Check duplicate symbols in hash map",
      find_symbol(&map, "test_symbol", VAR) != sym,
      "Duplicate symbols found"
    );
#endif

  return 0;
}
