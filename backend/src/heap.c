#include "heap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Tie-Breaking Rule:
 * In a Min Heap for Top-K, the root represents the "worst" item currently in the Top-K.
 * - Primary criterion: Lower frequency is worse (closer to root, evicted first).
 * - Secondary criterion: When frequencies are equal, lexicographically GREATER keyword
 *   is considered worse (so alphabetical 'apple' beats 'banana' if frequencies are equal).
 *
 * compare_heap_nodes(a, b):
 * Returns < 0 if a is "worse" than b (a < b in priority -> a closer to root of min-heap).
 * Returns > 0 if a is "better" than b (a > b in priority).
 * Returns 0 if equal.
 */
static int compare_heap_nodes(const HeapNode *a, const HeapNode *b) {
    if (a->frequency != b->frequency) {
        return a->frequency - b->frequency; /* Lower frequency is smaller */
    }
    /* Same frequency: strcmp > 0 means a is lexicographically greater, so considered smaller priority */
    return strcmp(b->keyword, a->keyword);
}

/* Allocate and initialize Min Heap */
MinHeap* minheap_create(size_t capacity) {
    if (capacity == 0) return NULL;

    MinHeap *heap = (MinHeap *)malloc(sizeof(MinHeap));
    if (!heap) return NULL;

    heap->elements = (HeapNode *)calloc(capacity, sizeof(HeapNode));
    if (!heap->elements) {
        free(heap);
        return NULL;
    }

    heap->capacity = capacity;
    heap->size = 0;
    return heap;
}

/* Free Min Heap and any copied strings inside */
void minheap_free(MinHeap *heap) {
    if (!heap) return;
    for (size_t i = 0; i < heap->size; i++) {
        if (heap->elements[i].keyword) {
            free(heap->elements[i].keyword);
        }
    }
    free(heap->elements);
    free(heap);
}

/* Swap two heap elements */
static void swap_nodes(HeapNode *a, HeapNode *b) {
    HeapNode temp = *a;
    *a = *b;
    *b = temp;
}

/* Sift up newly inserted element */
void minheap_heapify_up(MinHeap *heap, size_t index) {
    while (index > 0) {
        size_t parent = (index - 1) / 2;
        if (compare_heap_nodes(&heap->elements[index], &heap->elements[parent]) < 0) {
            swap_nodes(&heap->elements[index], &heap->elements[parent]);
            index = parent;
        } else {
            break;
        }
    }
}

/* Sift down root after replacement or extraction */
void minheap_heapify_down(MinHeap *heap, size_t index) {
    size_t smallest = index;
    size_t left = 2 * index + 1;
    size_t right = 2 * index + 2;

    if (left < heap->size && compare_heap_nodes(&heap->elements[left], &heap->elements[smallest]) < 0) {
        smallest = left;
    }

    if (right < heap->size && compare_heap_nodes(&heap->elements[right], &heap->elements[smallest]) < 0) {
        smallest = right;
    }

    if (smallest != index) {
        swap_nodes(&heap->elements[index], &heap->elements[smallest]);
        minheap_heapify_down(heap, smallest);
    }
}

/* Insert or conditionally replace minimum element if better candidate found */
bool minheap_insert_or_replace(MinHeap *heap, const char *keyword, int frequency) {
    if (!heap || !keyword) return false;

    HeapNode candidate;
    candidate.keyword = (char *)keyword;
    candidate.frequency = frequency;

    if (heap->size < heap->capacity) {
        /* Heap not full yet: insert at end and sift up */
        size_t idx = heap->size;
        heap->elements[idx].keyword = strdup(keyword);
        heap->elements[idx].frequency = frequency;
        heap->size++;
        minheap_heapify_up(heap, idx);
        return true;
    }

    /* Heap is full (size == capacity): compare with root (the minimum) */
    if (compare_heap_nodes(&candidate, &heap->elements[0]) > 0) {
        /* Candidate is strictly better than the current minimum: replace root */
        free(heap->elements[0].keyword);
        heap->elements[0].keyword = strdup(keyword);
        heap->elements[0].frequency = frequency;
        minheap_heapify_down(heap, 0);
        return true;
    }

    /* Candidate was not better than current Top-K */
    return false;
}

/* Extract root minimum */
HeapNode minheap_extract_min(MinHeap *heap) {
    HeapNode empty = { NULL, 0 };
    if (!heap || heap->size == 0) return empty;

    HeapNode min_node = heap->elements[0];
    heap->elements[0] = heap->elements[heap->size - 1];
    heap->size--;

    if (heap->size > 0) {
        minheap_heapify_down(heap, 0);
    }

    return min_node;
}

/* Peek at root minimum */
const HeapNode* minheap_peek_min(const MinHeap *heap) {
    if (!heap || heap->size == 0) return NULL;
    return &heap->elements[0];
}

/*
 * Extract all elements from heap in DESCENDING order of popularity (rank 1 to rank K).
 * This empties the heap.
 */
size_t minheap_extract_sorted_descending(MinHeap *heap, HeapNode **out_results) {
    if (!heap || !out_results) return 0;

    size_t count = heap->size;
    if (count == 0) {
        *out_results = NULL;
        return 0;
    }

    HeapNode *results = (HeapNode *)malloc(count * sizeof(HeapNode));
    if (!results) return 0;

    /* Repeatedly extract min: gives ascending order, so fill from back to front */
    for (size_t i = 0; i < count; i++) {
        size_t pos = count - 1 - i;
        results[pos] = minheap_extract_min(heap);
    }

    *out_results = results;
    return count;
}

/* Rank an arbitrary collection of matches using a Min Heap of size K */
size_t minheap_rank_top_k(const KeywordMatch *matches, size_t match_count, size_t k, HeapNode **out_top_k) {
    if (!matches || match_count == 0 || k == 0 || !out_top_k) {
        if (out_top_k) *out_top_k = NULL;
        return 0;
    }

    MinHeap *heap = minheap_create(k);
    if (!heap) return 0;

    for (size_t i = 0; i < match_count; i++) {
        minheap_insert_or_replace(heap, matches[i].keyword, matches[i].frequency);
    }

    size_t count = minheap_extract_sorted_descending(heap, out_top_k);
    minheap_free(heap);
    return count;
}

/* Free results returned by rank_top_k or extract_sorted */
void minheap_free_results(HeapNode *results, size_t count) {
    if (!results) return;
    for (size_t i = 0; i < count; i++) {
        if (results[i].keyword) {
            free(results[i].keyword);
        }
    }
    free(results);
}

