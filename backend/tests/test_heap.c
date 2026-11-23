#include "heap.h"
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

void test_minheap_lifecycle_and_basic_insert(void) {
    printf("\n--- Test Suite 1: Min Heap Creation & Basic Sifting ---\n");
    MinHeap *heap = minheap_create(5);
    TEST_ASSERT(heap != NULL, "minheap_create creates heap of capacity 5");
    TEST_ASSERT(heap->size == 0, "Initial size is 0");

    minheap_insert_or_replace(heap, "item c", 30);
    minheap_insert_or_replace(heap, "item a", 10);
    minheap_insert_or_replace(heap, "item b", 20);

    TEST_ASSERT(heap->size == 3, "Size is 3 after insertions");
    const HeapNode *min = minheap_peek_min(heap);
    TEST_ASSERT(min != NULL && min->frequency == 10 && strcmp(min->keyword, "item a") == 0,
                "Root correctly maintains minimum element (freq 10)");

    minheap_free(heap);
}

void test_minheap_top_k_replacement(void) {
    printf("\n--- Test Suite 2: Top-K Replacement on Full Heap ---\n");
    MinHeap *heap = minheap_create(3);

    minheap_insert_or_replace(heap, "low", 10);
    minheap_insert_or_replace(heap, "med", 50);
    minheap_insert_or_replace(heap, "high", 100);

    /* Heap is now full (size 3, min is 10) */
    TEST_ASSERT(minheap_peek_min(heap)->frequency == 10, "Minimum is 10");

    /* Try inserting worse element (frequency 5) - should be rejected */
    bool accepted = minheap_insert_or_replace(heap, "worse", 5);
    TEST_ASSERT(!accepted, "Candidate with frequency 5 < 10 was correctly rejected");
    TEST_ASSERT(heap->size == 3, "Heap size remains 3");

    /* Insert better element (frequency 75) - should replace 10 */
    accepted = minheap_insert_or_replace(heap, "better", 75);
    TEST_ASSERT(accepted, "Candidate with frequency 75 replaced root");
    TEST_ASSERT(minheap_peek_min(heap)->frequency == 50, "New minimum is now 50 ('med')");

    minheap_free(heap);
}

void test_minheap_descending_extraction(void) {
    printf("\n--- Test Suite 3: Descending Extraction & Ordering ---\n");
    MinHeap *heap = minheap_create(4);

    minheap_insert_or_replace(heap, "alpha", 40);
    minheap_insert_or_replace(heap, "beta", 80);
    minheap_insert_or_replace(heap, "gamma", 20);
    minheap_insert_or_replace(heap, "delta", 60);

    HeapNode *sorted = NULL;
    size_t count = minheap_extract_sorted_descending(heap, &sorted);

    TEST_ASSERT(count == 4, "Extracted all 4 elements");
    TEST_ASSERT(sorted[0].frequency == 80 && strcmp(sorted[0].keyword, "beta") == 0, "Rank 1 is beta (freq 80)");
    TEST_ASSERT(sorted[1].frequency == 60 && strcmp(sorted[1].keyword, "delta") == 0, "Rank 2 is delta (freq 60)");
    TEST_ASSERT(sorted[2].frequency == 40 && strcmp(sorted[2].keyword, "alpha") == 0, "Rank 3 is alpha (freq 40)");
    TEST_ASSERT(sorted[3].frequency == 20 && strcmp(sorted[3].keyword, "gamma") == 0, "Rank 4 is gamma (freq 20)");

    minheap_free_results(sorted, count);
    minheap_free(heap);
}

void test_minheap_tie_breaking(void) {
    printf("\n--- Test Suite 4: Equal Frequency Tie-Breaking ---\n");
    MinHeap *heap = minheap_create(2);

    /* Two items with exact same frequency: 'banana' and 'apple' */
    minheap_insert_or_replace(heap, "banana", 50);
    minheap_insert_or_replace(heap, "apple", 50);

    /* By our rule, 'apple' has higher priority (earlier alphabetical),
       so 'banana' is considered the min and sits at root */
    TEST_ASSERT(strcmp(minheap_peek_min(heap)->keyword, "banana") == 0,
                "Tie-break: 'banana' (lexicographically greater) is root of min-heap");

    HeapNode *sorted = NULL;
    size_t count = minheap_extract_sorted_descending(heap, &sorted);
    TEST_ASSERT(count == 2, "Extracted 2 items");
    TEST_ASSERT(strcmp(sorted[0].keyword, "apple") == 0, "Rank 1 is 'apple' (earlier alphabetically)");
    TEST_ASSERT(strcmp(sorted[1].keyword, "banana") == 0, "Rank 2 is 'banana'");

    minheap_free_results(sorted, count);
    minheap_free(heap);
}

void test_minheap_rank_top_k_integration(void) {
    printf("\n--- Test Suite 5: End-to-End Top-K with Many Matches ---\n");
    /* Suppose a prefix produces 15 matching keywords */
    KeywordMatch matches[15] = {
        {"python", 150}, {"programming", 90}, {"program", 85},
        {"project", 75}, {"product", 60}, {"processor", 40},
        {"protocol", 35}, {"profile", 30}, {"process", 25},
        {"probe", 20}, {"prototype", 18}, {"producer", 15},
        {"proficient", 12}, {"progress", 10}, {"proxy", 5}
    };

    HeapNode *top10 = NULL;
    size_t count = minheap_rank_top_k(matches, 15, 10, &top10);

    TEST_ASSERT(count == 10, "Top 10 selected from 15 candidates");
    TEST_ASSERT(strcmp(top10[0].keyword, "python") == 0 && top10[0].frequency == 150, "Rank 1 is 'python' (150)");
    TEST_ASSERT(strcmp(top10[1].keyword, "programming") == 0 && top10[1].frequency == 90, "Rank 2 is 'programming' (90)");
    TEST_ASSERT(strcmp(top10[9].keyword, "probe") == 0 && top10[9].frequency == 20, "Rank 10 is 'probe' (20)");

    /* Verify items with frequency <= 18 were excluded */
    bool excluded_prototype = true;
    bool excluded_proxy = true;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(top10[i].keyword, "prototype") == 0) excluded_prototype = false;
        if (strcmp(top10[i].keyword, "proxy") == 0) excluded_proxy = false;
    }
    TEST_ASSERT(excluded_prototype, "11th item 'prototype' (18) was properly excluded from Top 10");
    TEST_ASSERT(excluded_proxy, "Low frequency item 'proxy' (5) was properly excluded from Top 10");

    minheap_free_results(top10, count);
}

int main(void) {
    printf("====================================================\n");
    printf("        INTELLISUGGEST - MIN HEAP TEST SUITE        \n");
    printf("====================================================\n");

    test_minheap_lifecycle_and_basic_insert();
    test_minheap_top_k_replacement();
    test_minheap_descending_extraction();
    test_minheap_tie_breaking();
    test_minheap_rank_top_k_integration();

    printf("\n====================================================\n");
    printf("Test Results: %d / %d passed (%.1f%%)\n",
           passed_tests, total_tests,
           (total_tests > 0 ? (float)passed_tests * 100.0f / total_tests : 0.0f));
    printf("====================================================\n");

    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}
