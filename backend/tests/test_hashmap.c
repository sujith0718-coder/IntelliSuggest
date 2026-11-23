#include "hashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int total_tests = 0;
static int passed_tests = 0;

#define TEST_ASSERT(cond, msg) do { \
    total_tests++; \
    if (cond) { \
        passed_tests++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        printf("  [FAIL] %s (Line %d)\n", msg, __LINE__); \
    } \
} while(0)

void test_hashmap_lifecycle_and_insert(void) {
    printf("\n--- Test Suite 1: Hash Table Lifecycle & Insert ---\n");
    HashTable *ht = hashtable_create(16);
    TEST_ASSERT(ht != NULL, "hashtable_create creates table");
    TEST_ASSERT(ht->size == 0, "Initial size is 0");
    TEST_ASSERT(ht->capacity >= 64, "Capacity enforced minimum threshold (64)");

    TEST_ASSERT(hashtable_insert(ht, "python", 150, NULL, NULL, true), "Insert 'python' with freq 150");
    TEST_ASSERT(hashtable_insert(ht, "programming", 90, NULL, NULL, true), "Insert 'programming' with freq 90");
    TEST_ASSERT(hashtable_insert(ht, "database", 110, NULL, NULL, true), "Insert 'database' with freq 110");
    TEST_ASSERT(ht->size == 3, "Table size is 3");

    HashNode *node = hashtable_search(ht, "python");
    TEST_ASSERT(node != NULL, "Found 'python'");
    TEST_ASSERT(node->frequency == 150, "Frequency matches 150");
    TEST_ASSERT(strlen(node->created_at) > 0, "Created_at timestamp was automatically populated");

    /* Search non-existent */
    TEST_ASSERT(hashtable_search(ht, "unknown") == NULL, "Non-existent keyword returns NULL");

    hashtable_free(ht);
}

void test_hashmap_frequency_increments_and_updates(void) {
    printf("\n--- Test Suite 2: Frequency Increments & Updates ---\n");
    HashTable *ht = hashtable_create(64);

    hashtable_insert(ht, "algorithm", 50, NULL, NULL, true);

    int new_freq = 0;
    TEST_ASSERT(hashtable_increment_frequency(ht, "algorithm", "2026-11-23T23:00:00Z", &new_freq) && new_freq == 51,
                "Increment existing keyword frequency to 51");

    HashNode *node = hashtable_search(ht, "algorithm");
    TEST_ASSERT(node && node->frequency == 51, "Verified node frequency is 51");
    TEST_ASSERT(strcmp(node->last_searched_at, "2026-11-23T23:00:00Z") == 0, "Updated last_searched_at timestamp");

    /* Increment non-existent keyword - should automatically insert with frequency 1 */
    TEST_ASSERT(hashtable_increment_frequency(ht, "new query", NULL, &new_freq) && new_freq == 1,
                "Incrementing new keyword inserts with freq 1");
    TEST_ASSERT(hashtable_search(ht, "new query") != NULL, "New keyword exists in hash table");

    /* Manual frequency update */
    TEST_ASSERT(hashtable_update_frequency(ht, "algorithm", 100, NULL), "Manual update frequency to 100");
    node = hashtable_search(ht, "algorithm");
    TEST_ASSERT(node && node->frequency == 100, "Verified manual update frequency is 100");

    hashtable_free(ht);
}

void test_hashmap_collision_and_resizing(void) {
    printf("\n--- Test Suite 3: Collision Handling & Dynamic Resizing ---\n");
    HashTable *ht = hashtable_create(64);

    /* Insert 100 distinct alphabetic items to trigger automatic rehashing */
    char buf[64];
    for (int i = 0; i < 100; i++) {
        snprintf(buf, sizeof(buf), "item %c%c", 'a' + (i / 26), 'a' + (i % 26));
        hashtable_insert(ht, buf, i * 2, NULL, NULL, true);
    }

    TEST_ASSERT(ht->size == 100, "Successfully inserted 100 items");
    TEST_ASSERT(ht->capacity > 64, "Table dynamically resized beyond initial 64 buckets");
    TEST_ASSERT(hashtable_get_load_factor(ht) <= HASH_MAX_LOAD_FACTOR, "Load factor remains below max load factor");

    /* Verify all 100 items are still correctly retrievable */
    bool all_found = true;
    for (int i = 0; i < 100; i++) {
        snprintf(buf, sizeof(buf), "item %c%c", 'a' + (i / 26), 'a' + (i % 26));
        HashNode *n = hashtable_search(ht, buf);
        if (!n || n->frequency != i * 2) {
            all_found = false;
            break;
        }
    }
    TEST_ASSERT(all_found, "All 100 rehashed items found with exact matching frequencies");

    hashtable_free(ht);
}

void test_hashmap_deletion(void) {
    printf("\n--- Test Suite 4: Deletion & Bucket Cleanup ---\n");
    HashTable *ht = hashtable_create(64);

    hashtable_insert(ht, "item one", 10, NULL, NULL, true);
    hashtable_insert(ht, "item two", 20, NULL, NULL, true);

    TEST_ASSERT(hashtable_delete(ht, "item one"), "Delete 'item one'");
    TEST_ASSERT(hashtable_search(ht, "item one") == NULL, "'item one' is no longer in hash table");
    TEST_ASSERT(hashtable_search(ht, "item two") != NULL, "'item two' is unaffected");
    TEST_ASSERT(ht->size == 1, "Hash table size decreased to 1");

    /* Delete non-existent item */
    TEST_ASSERT(!hashtable_delete(ht, "item one"), "Deleting already deleted item returns false");

    hashtable_free(ht);
}

int main(void) {
    printf("====================================================\n");
    printf("       INTELLISUGGEST - HASH TABLE TEST SUITE        \n");
    printf("====================================================\n");

    test_hashmap_lifecycle_and_insert();
    test_hashmap_frequency_increments_and_updates();
    test_hashmap_collision_and_resizing();
    test_hashmap_deletion();

    printf("\n====================================================\n");
    printf("Test Results: %d / %d passed (%.1f%%)\n",
           passed_tests, total_tests,
           (total_tests > 0 ? (float)passed_tests * 100.0f / total_tests : 0.0f));
    printf("====================================================\n");

    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}
