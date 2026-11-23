#include "concept_index.h"
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

void test_tokenization_and_stop_words(void) {
    printf("\n--- Test Suite 1: Tokenization & Stop-Word Filtering ---\n");
    char tokens[MAX_TOKENS_PER_QUERY][64];

    size_t count = tokenize_and_filter("Technique used to store previously calculated results", tokens);
    TEST_ASSERT(count > 0, "Extracted informative tokens");

    bool has_store = false;
    bool has_calculated = false;
    bool has_results = false;
    bool has_to = false; /* Stop word */

    for (size_t i = 0; i < count; i++) {
        if (strcmp(tokens[i], "store") == 0) has_store = true;
        if (strcmp(tokens[i], "calculated") == 0) has_calculated = true;
        if (strcmp(tokens[i], "results") == 0) has_results = true;
        if (strcmp(tokens[i], "to") == 0) has_to = true;
    }

    TEST_ASSERT(has_store && has_calculated && has_results, "Kept meaningful content words");
    TEST_ASSERT(!has_to, "Filtered out English stop-word 'to'");
}

void test_direct_concept_queries(void) {
    printf("\n--- Test Suite 2: Descriptive Concept Retrieval ---\n");
    ConceptIndex *idx = concept_index_create();
    concept_index_seed_defaults(idx);

    ConceptSearchResult *results = NULL;

    /* 1. Memoization query */
    size_t count = concept_index_search(idx, "technique used to store previously calculated results", 3, &results);
    TEST_ASSERT(count > 0, "Query returned matches for calculation storage query");
    TEST_ASSERT(strcmp(results[0].concept_name, "Memoization") == 0, "Top result is 'Memoization'");
    TEST_ASSERT(results[0].relevance_score > 0.4, "High relevance score (> 0.4)");
    concept_index_free_results(results);

    /* 2. HDMI Adapter query */
    count = concept_index_search(idx, "something used to connect a laptop to a projector", 3, &results);
    TEST_ASSERT(count > 0, "Query returned matches for laptop to projector connection");
    TEST_ASSERT(strcmp(results[0].concept_name, "HDMI Adapter") == 0, "Top result is 'HDMI Adapter'");
    concept_index_free_results(results);

    /* 3. Shortest path query */
    count = concept_index_search(idx, "algorithm that finds the shortest path in a graph", 3, &results);
    TEST_ASSERT(count > 0, "Query returned matches for shortest path");
    TEST_ASSERT(strcmp(results[0].concept_name, "Dijkstra Algorithm") == 0, "Top result is 'Dijkstra Algorithm'");
    concept_index_free_results(results);

    /* 4. Laptop cooling query */
    count = concept_index_search(idx, "something used to cool a laptop and prevent overheating", 3, &results);
    TEST_ASSERT(count > 0, "Query returned matches for laptop cooling");
    TEST_ASSERT(strcmp(results[0].concept_name, "Laptop Cooling Pad") == 0, "Top result is 'Laptop Cooling Pad'");
    concept_index_free_results(results);

    concept_index_free(idx);
}

void test_synonyms_and_edge_cases(void) {
    printf("\n--- Test Suite 3: Synonyms, Incomplete & No-Match Queries ---\n");
    ConceptIndex *idx = concept_index_create();
    concept_index_seed_defaults(idx);

    ConceptSearchResult *results = NULL;

    /* Synonym search */
    size_t count = concept_index_search(idx, "dynamic programming recursion optimization caching", 3, &results);
    TEST_ASSERT(count > 0 && strcmp(results[0].concept_name, "Memoization") == 0, "Synonym query matched 'Memoization'");
    concept_index_free_results(results);

    /* Incomplete phrasing */
    count = concept_index_search(idx, "divide array in half", 3, &results);
    TEST_ASSERT(count > 0 && strcmp(results[0].concept_name, "Binary Search") == 0, "Incomplete description matched 'Binary Search'");
    concept_index_free_results(results);

    /* No-match query */
    count = concept_index_search(idx, "supercalifragilistic quantum submarine flying saucer", 3, &results);
    TEST_ASSERT(count == 0, "Irrelevant query correctly returns 0 results");
    concept_index_free_results(results);

    concept_index_free(idx);
}

int main(void) {
    printf("====================================================\n");
    printf("   INTELLISUGGEST - FORGOTTEN SEARCH TEST SUITE     \n");
    printf("====================================================\n");

    test_tokenization_and_stop_words();
    test_direct_concept_queries();
    test_synonyms_and_edge_cases();

    printf("\n====================================================\n");
    printf("Test Results: %d / %d passed (%.1f%%)\n",
           passed_tests, total_tests,
           (total_tests > 0 ? (float)passed_tests * 100.0f / total_tests : 0.0f));
    printf("====================================================\n");

    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}
