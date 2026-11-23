#include "ranking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
static double get_hires_time_us(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000000.0 / (double)freq.QuadPart;
}
#else
static double get_hires_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000000.0 + (double)ts.tv_nsec / 1000.0;
}
#endif

/*
 * Approach A: Uncached query-time ranking.
 * 1. Find prefix node in Trie: O(L)
 * 2. DFS collect all matching words in subtree: O(M)
 * 3. Feed matches into Min Heap of size K: O(M log K)
 * 4. Extract Top-K in descending order: O(K log K)
 */
size_t ranking_get_suggestions_uncached(const Trie *trie, const char *prefix, size_t k, SuggestionItem **out_suggestions) {
    if (!trie || !out_suggestions || k == 0) {
        if (out_suggestions) *out_suggestions = NULL;
        return 0;
    }

    KeywordMatch *matches = NULL;
    size_t match_count = trie_collect_matches(trie, prefix, &matches);
    if (match_count == 0) {
        *out_suggestions = NULL;
        return 0;
    }

    HeapNode *top_k = NULL;
    size_t result_count = minheap_rank_top_k(matches, match_count, k, &top_k);
    trie_free_matches(matches, match_count);

    if (result_count == 0) {
        *out_suggestions = NULL;
        return 0;
    }

    SuggestionItem *suggestions = (SuggestionItem *)malloc(result_count * sizeof(SuggestionItem));
    if (!suggestions) {
        minheap_free_results(top_k, result_count);
        *out_suggestions = NULL;
        return 0;
    }

    for (size_t i = 0; i < result_count; i++) {
        strncpy(suggestions[i].keyword, top_k[i].keyword, sizeof(suggestions[i].keyword) - 1);
        suggestions[i].keyword[sizeof(suggestions[i].keyword) - 1] = '\0';
        suggestions[i].frequency = top_k[i].frequency;
    }

    minheap_free_results(top_k, result_count);
    *out_suggestions = suggestions;
    return result_count;
}

/*
 * Helper: Recomputes the top10_cache for a single TrieNode by inspecting its subtree.
 */
void ranking_compute_node_cache(TrieNode *node, const Trie *trie) {
    if (!node || !trie) return;

    KeywordMatch *matches = NULL;
    size_t match_count = 0;

    /* We collect all matches in this subtree */
    Trie sub_trie;
    sub_trie.root = node;
    sub_trie.total_nodes = 0;
    sub_trie.total_words = 0;

    match_count = trie_collect_matches(&sub_trie, "", &matches);
    if (match_count == 0) {
        node->cache_count = 0;
        node->is_cache_valid = true;
        return;
    }

    HeapNode *top_k = NULL;
    size_t result_count = minheap_rank_top_k(matches, match_count, TOP_K_SIZE, &top_k);
    trie_free_matches(matches, match_count);

    node->cache_count = result_count;
    for (size_t i = 0; i < result_count; i++) {
        strncpy(node->top10_cache[i].keyword, top_k[i].keyword, sizeof(node->top10_cache[i].keyword) - 1);
        node->top10_cache[i].keyword[sizeof(node->top10_cache[i].keyword) - 1] = '\0';
        node->top10_cache[i].frequency = top_k[i].frequency;
    }

    minheap_free_results(top_k, result_count);
    node->is_cache_valid = true;
}

/*
 * Approach B: Cached retrieval.
 * 1. Find prefix node: O(L)
 * 2. If node's cache is valid, copy directly: O(K)
 * 3. Total time: O(L + K)
 */
size_t ranking_get_suggestions_cached(Trie *trie, const char *prefix, size_t k, SuggestionItem **out_suggestions) {
    if (!trie || !out_suggestions || k == 0) {
        if (out_suggestions) *out_suggestions = NULL;
        return 0;
    }

    TrieNode *node = NULL;
    if (!prefix || strlen(prefix) == 0) {
        node = trie->root;
    } else {
        node = trie_find_node(trie, prefix);
    }

    if (!node) {
        *out_suggestions = NULL;
        return 0;
    }

    /* If cache is not valid, compute it on-demand */
    if (!node->is_cache_valid) {
        ranking_compute_node_cache(node, trie);
    }

    size_t return_count = (k < node->cache_count) ? k : node->cache_count;
    if (return_count == 0) {
        *out_suggestions = NULL;
        return 0;
    }

    SuggestionItem *suggestions = (SuggestionItem *)malloc(return_count * sizeof(SuggestionItem));
    if (!suggestions) {
        *out_suggestions = NULL;
        return 0;
    }

    for (size_t i = 0; i < return_count; i++) {
        strncpy(suggestions[i].keyword, node->top10_cache[i].keyword, sizeof(suggestions[i].keyword) - 1);
        suggestions[i].keyword[sizeof(suggestions[i].keyword) - 1] = '\0';
        suggestions[i].frequency = node->top10_cache[i].frequency;
    }

    *out_suggestions = suggestions;
    return return_count;
}

/*
 * Cache propagation along the path from root to keyword leaf:
 * When frequency changes, each ancestor's Top-10 cache must be updated.
 */
void ranking_propagate_cache_update(Trie *trie, const char *keyword, int new_frequency) {
    if (!trie || !trie->root || !keyword) return;

    char *norm = trie_normalize(keyword);
    if (!norm) return;

    size_t len = strlen(norm);

    /* Array of nodes along the path */
    TrieNode **path = (TrieNode **)malloc((len + 1) * sizeof(TrieNode *));
    if (!path) {
        free(norm);
        return;
    }

    path[0] = trie->root;
    size_t depth = 0;
    TrieNode *curr = trie->root;

    for (size_t i = 0; i < len; i++) {
        int idx = trie_char_to_index(norm[i]);
        if (idx < 0 || !curr->children[idx]) {
            break;
        }
        curr = curr->children[idx];
        depth++;
        path[depth] = curr;
    }

    /* Update from leaf upwards to root */
    for (int i = (int)depth; i >= 0; i--) {
        ranking_compute_node_cache(path[i], trie);
    }

    free(path);
    free(norm);
}

/* Cache propagation when a keyword is deleted */
void ranking_propagate_cache_deletion(Trie *trie, const char *keyword) {
    if (!trie || !trie->root || !keyword) return;

    char *norm = trie_normalize(keyword);
    if (!norm) return;

    size_t len = strlen(norm);
    TrieNode *curr = trie->root;

    /* Invalidate along the path */
    trie->root->is_cache_valid = false;
    for (size_t i = 0; i < len; i++) {
        int idx = trie_char_to_index(norm[i]);
        if (idx < 0 || !curr->children[idx]) {
            break;
        }
        curr = curr->children[idx];
        curr->is_cache_valid = false;
    }

    free(norm);
}

/* Recursive helper to rebuild cache for an entire subtree (post-order traversal) */
static void rebuild_cache_recursive(TrieNode *node, const Trie *trie) {
    if (!node) return;
    for (int i = 0; i < TRIE_ALPHABET_SIZE; i++) {
        if (node->children[i]) {
            rebuild_cache_recursive(node->children[i], trie);
        }
    }
    ranking_compute_node_cache(node, trie);
}

/* Rebuild caches for all active nodes in the Trie */
void ranking_rebuild_entire_cache(Trie *trie) {
    if (!trie || !trie->root) return;
    rebuild_cache_recursive(trie->root, trie);
}

/* Unified suggestion getter with high-resolution microsecond latency measurement */
size_t ranking_get_suggestions(Trie *trie, const char *prefix, size_t k, RankingMode mode, SuggestionItem **out_suggestions, double *out_latency_us) {
    double t_start = get_hires_time_us();
    size_t count = 0;

    if (mode == RANKING_MODE_CACHED) {
        count = ranking_get_suggestions_cached(trie, prefix, k, out_suggestions);
    } else {
        count = ranking_get_suggestions_uncached(trie, prefix, k, out_suggestions);
    }

    double t_end = get_hires_time_us();
    if (out_latency_us) {
        *out_latency_us = (t_end - t_start);
    }

    return count;
}

/* Free suggestions array */
void ranking_free_suggestions(SuggestionItem *suggestions) {
    if (suggestions) {
        free(suggestions);
    }
}

