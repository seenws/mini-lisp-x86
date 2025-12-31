// test_hashmap.c
// Compile with: gcc -o test_hashmap hashmap.c test_hashmap.c -I. (assuming hashmap.h in same dir)

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "../../compiler/hashmap.h"
#include "../../util/mlispc_strdup.h"

void run_tests() {
    printf("Running hashmap unit tests...\n");

    // Test 1: Creation and basic insert/lookup
    struct u32hashmap *hm = hashmap_new(0);  // Min size 16
    assert(hm != NULL);
    assert(hm->nbuckets == 16);
    assert(hm->nfilled == 0);

    int res;
    void *val;

    res = hashmap_insert(hm, "key1", (void *)"value1");
    assert(res == 0);
    assert(hm->nfilled == 1);

    res = hashmap_lookup(hm, "key1", &val);
    assert(res == 1);
    assert(strcmp((char *)val, "value1") == 0);

    // Test 2: Update existing key
    res = hashmap_insert(hm, "key1", (void *)"new_value1");
    assert(res == 0);
    assert(hm->nfilled == 1);  // No increase

    res = hashmap_lookup(hm, "key1", &val);
    assert(res == 1);
    assert(strcmp((char *)val, "new_value1") == 0);

    // Test 3: Multiple inserts and lookup not found
    res = hashmap_insert(hm, "key2", (void *)"value2");
    assert(res == 0);
    res = hashmap_insert(hm, "key3", (void *)"value3");
    assert(res == 0);
    assert(hm->nfilled == 3);

    res = hashmap_lookup(hm, "nonexistent", &val);
    assert(res == 0);
    assert(val == NULL);

    // Test 4: Remove
    res = hashmap_remove(hm, "key2");
    assert(res == 0);
    assert(hm->nfilled == 2);

    res = hashmap_lookup(hm, "key2", &val);
    assert(res == 0);
    assert(val == NULL);

    res = hashmap_remove(hm, "nonexistent");  // No-op
    assert(res == 0);
    assert(hm->nfilled == 2);

    // Test 5: Resize (insert enough to trigger >0.75 load, 16*0.75=12)
    for (int i = 4; i <= 15; i++) {
        char key[10];
        snprintf(key, sizeof(key), "key%d", i);
        res = hashmap_insert(hm, key, (void *)"val");
        assert(res == 0);
    }
    assert(hm->nfilled == 14);  // 2 previous + 12 new
    assert(hm->nbuckets == 32);  // Should have resized

    // Check a post-resize lookup
    res = hashmap_lookup(hm, "key1", &val);
    assert(res == 1);
    assert(strcmp((char *)val, "new_value1") == 0);

    // Test 6: NULL handling
    res = hashmap_insert(NULL, "key", (void *)"val");
    assert(res == -1);
    res = hashmap_insert(hm, NULL, (void *)"val");
    assert(res == -1);
    res = hashmap_insert(hm, "key", NULL);
    assert(res == -1);

    res = hashmap_lookup(hm, NULL, &val);
    assert(res == -1);
    res = hashmap_lookup(NULL, "key", &val);
    assert(res == -1);
    res = hashmap_lookup(hm, "key", NULL);
    assert(res == -1);

    res = hashmap_remove(NULL, "key");
    assert(res == -1);
    res = hashmap_remove(hm, NULL);
    assert(res == -1);

    // Test 7: Empty key
    res = hashmap_insert(hm, "", (void *)"empty_key_val");
    assert(res == 0);
    res = hashmap_lookup(hm, "", &val);
    assert(res == 1);
    assert(strcmp((char *)val, "empty_key_val") == 0);

    // Test 8: Free
    hashmap_free(hm);
    // No asserts after free, but manual valgrind check recommended for leaks

    printf("All tests passed!\n");
}

int main() {
    run_tests();
    return 0;
}
