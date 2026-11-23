#ifndef HEAP_H
#define HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include "trie.h"

/* Element stored in the Min Heap */
typedef struct {
    char *keyword;
    int frequency;
} HeapNode;

/* Min Heap structure for Top-K ranking */
typedef struct {
    HeapNode *elements;
    size_t capacity;
    size_t size;
} MinHeap;

/* Lifecycle */
MinHeap* minheap_create(size_t capacity);
void minheap_free(MinHeap *heap);

/* Core Heap Operations */
void minheap_heapify_up(MinHeap *heap, size_t index);
void minheap_heapify_down(MinHeap *heap, size_t index);
bool minheap_insert_or_replace(MinHeap *heap, const char *keyword, int frequency);
HeapNode minheap_extract_min(MinHeap *heap);
const HeapNode* minheap_peek_min(const MinHeap *heap);

/* Top-K Selection Helper */
/* Returns results in DESCENDING order of frequency */
size_t minheap_extract_sorted_descending(MinHeap *heap, HeapNode **out_results);

/* End-to-end Top-K ranking for arbitrary match collections */
size_t minheap_rank_top_k(const KeywordMatch *matches, size_t match_count, size_t k, HeapNode **out_top_k);
void minheap_free_results(HeapNode *results, size_t count);

#endif /* HEAP_H */

