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
