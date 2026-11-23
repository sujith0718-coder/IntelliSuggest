#include "ranking.h"
#include "trie.h"
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

void test_approach_a_vs_approach_b_consistency(void) {
    printf("\n--- Test Suite 1: Approach A vs. Approach B Consistency ---\n");
    Trie *trie = trie_create();

    trie_insert(trie, "python", 150);
    trie_insert(trie, "programming", 90);
    trie_insert(trie, "program", 85);
    trie_insert(trie, "project", 75);
    trie_insert(trie, "product", 60);
    trie_insert(trie, "processor", 40);
    trie_insert(trie, "database", 110);
    trie_insert(trie, "data structures", 95);

    /* Build initial cache */
    ranking_rebuild_entire_cache(trie);

    /* Query prefix "pro" with both approaches */
    SuggestionItem *uncached = NULL;
    SuggestionItem *cached = NULL;
    double lat_uncached = 0.0;
    double lat_cached = 0.0;

    size_t count_a = ranking_get_suggestions(trie, "pro", 10, RANKING_MODE_HEAP, &uncached, &lat_uncached);
    size_t count_b = ranking_get_suggestions(trie, "pro", 10, RANKING_MODE_CACHED, &cached, &lat_cached);

    TEST_ASSERT(count_a == 5, "Approach A returned 5 matching words");
    TEST_ASSERT(count_b == 5, "Approach B returned 5 matching words");
    TEST_ASSERT(count_a == count_b, "Both approaches returned identical suggestion counts");

    bool identical_ranking = true;
    for (size_t i = 0; i < count_a; i++) {
        if (strcmp(uncached[i].keyword, cached[i].keyword) != 0 ||
            uncached[i].frequency != cached[i].frequency) {
            identical_ranking = false;
            break;
        }
    }
    TEST_ASSERT(identical_ranking, "Every suggestion and rank is 100% identical between Approach A and B");
    TEST_ASSERT(strcmp(cached[0].keyword, "programming") == 0 && cached[0].frequency == 90,
                "Top suggestion for 'pro' is 'programming' (freq 90)");
    TEST_ASSERT(strcmp(cached[1].keyword, "program") == 0 && cached[1].frequency == 85,
                "Second suggestion is 'program' (freq 85)");

    ranking_free_suggestions(uncached);
    ranking_free_suggestions(cached);
    trie_free(trie);
}

void test_cache_propagation_on_frequency_update(void) {
    printf("\n--- Test Suite 2: Frequency Propagation in Top-10 Cache ---\n");
    Trie *trie = trie_create();

    trie_insert(trie, "programming", 90);
    trie_insert(trie, "program", 85);
    trie_insert(trie, "project", 75);
    trie_insert(trie, "processor", 40);

    ranking_rebuild_entire_cache(trie);

    /* Initially, processor is at the bottom (rank 4, freq 40) */
    SuggestionItem *cached = NULL;
    size_t count = ranking_get_suggestions(trie, "pro", 10, RANKING_MODE_CACHED, &cached, NULL);
    TEST_ASSERT(count == 4, "Initial count is 4");
    TEST_ASSERT(strcmp(cached[3].keyword, "processor") == 0, "'processor' initially at bottom");
    ranking_free_suggestions(cached);

    /* Update frequency of 'processor' to 120 (higher than 'programming') */
    trie_update_frequency(trie, "processor", 120);
    ranking_propagate_cache_update(trie, "processor", 120);

    /* Re-query cached suggestions */
    count = ranking_get_suggestions(trie, "pro", 10, RANKING_MODE_CACHED, &cached, NULL);
    TEST_ASSERT(count == 4, "Count remains 4");
    TEST_ASSERT(strcmp(cached[0].keyword, "processor") == 0 && cached[0].frequency == 120,
                "Cache propagated: 'processor' promoted to Rank 1 with frequency 120");
    TEST_ASSERT(strcmp(cached[1].keyword, "programming") == 0 && cached[1].frequency == 90,
                "'programming' shifted to Rank 2");

    ranking_free_suggestions(cached);
    trie_free(trie);
}

void test_cache_maintenance_on_insertion_and_deletion(void) {
    printf("\n--- Test Suite 3: Insertion & Deletion Cache Maintenance ---\n");
    Trie *trie = trie_create();

    trie_insert(trie, "alpha", 10);
    trie_insert(trie, "beta", 20);
    ranking_rebuild_entire_cache(trie);

    /* Insert new word with high frequency */
    trie_insert(trie, "best", 100);
    ranking_propagate_cache_update(trie, "best", 100);

    SuggestionItem *cached = NULL;
    size_t count = ranking_get_suggestions(trie, "b", 10, RANKING_MODE_CACHED, &cached, NULL);
    TEST_ASSERT(count == 2, "Prefix 'b' matches 2 words ('best' and 'beta')");
    TEST_ASSERT(strcmp(cached[0].keyword, "best") == 0 && cached[0].frequency == 100,
                "New keyword 'best' immediately present at Rank 1 in cache");
    ranking_free_suggestions(cached);

    /* Delete 'best' */
    ranking_propagate_cache_deletion(trie, "best");
    trie_delete(trie, "best");

    count = ranking_get_suggestions(trie, "b", 10, RANKING_MODE_CACHED, &cached, NULL);
    TEST_ASSERT(count == 1, "After deletion, prefix 'b' matches 1 word");
    TEST_ASSERT(strcmp(cached[0].keyword, "beta") == 0, "'beta' is now Rank 1");
    ranking_free_suggestions(cached);

    trie_free(trie);
}

int main(void) {
    printf("====================================================\n");
    printf("    INTELLISUGGEST - TOP-10 CACHE TEST SUITE        \n");
    printf("====================================================\n");

    test_approach_a_vs_approach_b_consistency();
    test_cache_propagation_on_frequency_update();
    test_cache_maintenance_on_insertion_and_deletion();

    printf("\n====================================================\n");
    printf("Test Results: %d / %d passed (%.1f%%)\n",
           passed_tests, total_tests,
           (total_tests > 0 ? (float)passed_tests * 100.0f / total_tests : 0.0f));
    printf("====================================================\n");

    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}

