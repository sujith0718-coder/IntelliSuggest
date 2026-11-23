#include "trie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

void test_trie_lifecycle_and_normalization(void) {
    printf("\n--- Test Suite 1: Trie Lifecycle & Normalization ---\n");
    Trie *trie = trie_create();
    TEST_ASSERT(trie != NULL, "trie_create returns valid non-null pointer");
    TEST_ASSERT(trie->root != NULL, "trie->root is initialized");
    TEST_ASSERT(trie->total_words == 0, "Initial word count is 0");
    TEST_ASSERT(trie->total_nodes == 1, "Initial node count is 1 (root node)");

    char *norm1 = trie_normalize("  PyThoN   Data Structures  ");
    TEST_ASSERT(strcmp(norm1, "python data structures") == 0, "Normalizes case and collapses whitespace");
    free(norm1);

    char *norm2 = trie_normalize("C++ & Algorithms-101!");
    TEST_ASSERT(strcmp(norm2, "c algorithms 101") == 0, "Preserves digits in technical alphanumeric keywords");
    free(norm2);

    char *norm3 = trie_normalize("SHA-256 Hash & IPv6 Addressing / HTTP/2");
    TEST_ASSERT(strcmp(norm3, "sha 256 hash ipv6 addressing http 2") == 0, "Normalizes technical terms with digits and slashes");
    free(norm3);

    trie_free(trie);
    TEST_ASSERT(true, "trie_free executed cleanly without memory crash");
}

void test_trie_insert_and_exact_search(void) {
    printf("\n--- Test Suite 2: Insertion & Exact Search ---\n");
    Trie *trie = trie_create();

    TEST_ASSERT(trie_insert(trie, "python", 150), "Insert 'python' with frequency 150");
    TEST_ASSERT(trie_insert(trie, "programming", 90), "Insert 'programming' with frequency 90");
    TEST_ASSERT(trie_insert(trie, "program", 85), "Insert prefix word 'program' with frequency 85");
    TEST_ASSERT(trie_insert(trie, "project", 75), "Insert 'project' with frequency 75");
    TEST_ASSERT(trie->total_words == 4, "Total words in trie is 4");

    int freq = 0;
    TEST_ASSERT(trie_search_exact(trie, "python", &freq) && freq == 150, "Exact search 'python' matches frequency 150");
    TEST_ASSERT(trie_search_exact(trie, "PYTHON", &freq) && freq == 150, "Case-insensitive search matches 'python'");
    TEST_ASSERT(trie_search_exact(trie, "program", &freq) && freq == 85, "Exact search for prefix-word 'program'");
    TEST_ASSERT(trie_search_exact(trie, "programming", &freq) && freq == 90, "Exact search for longer word 'programming'");
    TEST_ASSERT(!trie_search_exact(trie, "pro", &freq), "'pro' is only a prefix, not a complete keyword");
    TEST_ASSERT(!trie_search_exact(trie, "java", &freq), "'java' does not exist in trie");

    /* Update frequency */
    TEST_ASSERT(trie_update_frequency(trie, "python", 200), "Update frequency of 'python' to 200");
    TEST_ASSERT(trie_search_exact(trie, "python", &freq) && freq == 200, "Verified updated frequency is 200");

    trie_free(trie);
}

void test_trie_prefix_collection(void) {
    printf("\n--- Test Suite 3: Prefix Subtree Collection ---\n");
    Trie *trie = trie_create();

    trie_insert(trie, "program", 85);
    trie_insert(trie, "programming", 90);
    trie_insert(trie, "project", 75);
    trie_insert(trie, "product", 60);
    trie_insert(trie, "processor", 40);
    trie_insert(trie, "database", 110);
    trie_insert(trie, "data science", 70);

    KeywordMatch *matches = NULL;
    size_t count = trie_collect_matches(trie, "pro", &matches);
    TEST_ASSERT(count == 5, "Prefix 'pro' correctly matches 5 words");

    bool found_program = false;
    bool found_processor = false;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(matches[i].keyword, "program") == 0) found_program = true;
        if (strcmp(matches[i].keyword, "processor") == 0) found_processor = true;
    }
    TEST_ASSERT(found_program && found_processor, "Collected matches contain expected words");
    trie_free_matches(matches, count);

    /* Test empty prefix matches all words */
    count = trie_collect_matches(trie, "", &matches);
    TEST_ASSERT(count == 7, "Empty prefix matches all 7 words in trie");
    trie_free_matches(matches, count);

    /* Test nonexistent prefix */
    count = trie_collect_matches(trie, "xyz", &matches);
    TEST_ASSERT(count == 0, "Nonexistent prefix 'xyz' yields 0 matches");
    trie_free_matches(matches, count);

    trie_free(trie);
}

void test_trie_deletion(void) {
    printf("\n--- Test Suite 4: Trie Deletion & Node Cleanup ---\n");
    Trie *trie = trie_create();

    trie_insert(trie, "program", 85);
    trie_insert(trie, "programming", 90);
    trie_insert(trie, "project", 75);

    /* Delete 'programming' - 'program' should remain intact */
    TEST_ASSERT(trie_delete(trie, "programming"), "Deleted 'programming'");
    TEST_ASSERT(!trie_search_exact(trie, "programming", NULL), "'programming' no longer exists");
    TEST_ASSERT(trie_search_exact(trie, "program", NULL), "Prefix word 'program' is preserved");

    /* Delete 'program' */
    TEST_ASSERT(trie_delete(trie, "program"), "Deleted 'program'");
    TEST_ASSERT(!trie_search_exact(trie, "program", NULL), "'program' no longer exists");
    TEST_ASSERT(trie_search_exact(trie, "project", NULL), "'project' is preserved");

    /* Try deleting already deleted word */
    TEST_ASSERT(!trie_delete(trie, "program"), "Deleting non-existent word returns false");

    trie_free(trie);
}

int main(void) {
    printf("====================================================\n");
    printf("         INTELLISUGGEST - TRIE MODULE TEST SUITE     \n");
    printf("====================================================\n");

    test_trie_lifecycle_and_normalization();
    test_trie_insert_and_exact_search();
    test_trie_prefix_collection();
    test_trie_deletion();

    printf("\n====================================================\n");
    printf("Test Results: %d / %d passed (%.1f%%)\n",
           passed_tests, total_tests,
           (total_tests > 0 ? (float)passed_tests * 100.0f / total_tests : 0.0f));
    printf("====================================================\n");

    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}

