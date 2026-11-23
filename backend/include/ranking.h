#ifndef RANKING_H
#define RANKING_H

#include "trie.h"
#include "heap.h"
#include "hashmap.h"
#include <stdbool.h>
#include <stddef.h>

/* Result item returned to callers */
typedef struct {
    char keyword[128];
    int frequency;
} SuggestionItem;

/* Search mode selector for comparisons */
typedef enum {
    RANKING_MODE_HEAP = 0,    /* Approach A: Trie traversal + Min Heap at query time */
    RANKING_MODE_CACHED = 1   /* Approach B: Precomputed Top-10 stored in Trie nodes */
} RankingMode;

/* Suggestion Retrieval */
/* Approach A: Uncached (Trie subtree DFS + Min Heap) */
size_t ranking_get_suggestions_uncached(const Trie *trie, const char *prefix, size_t k, SuggestionItem **out_suggestions);

/* Approach B: Cached (Direct lookup in Trie node's top10_cache) */
size_t ranking_get_suggestions_cached(Trie *trie, const char *prefix, size_t k, SuggestionItem **out_suggestions);

/* Unified suggestion interface supporting both modes */
size_t ranking_get_suggestions(Trie *trie, const char *prefix, size_t k, RankingMode mode, SuggestionItem **out_suggestions, double *out_latency_us);

/* Cache Maintenance & Propagation */
void ranking_compute_node_cache(TrieNode *node, const Trie *trie);
void ranking_propagate_cache_update(Trie *trie, const char *keyword, int new_frequency);
void ranking_propagate_cache_deletion(Trie *trie, const char *keyword);
void ranking_rebuild_entire_cache(Trie *trie);

/* Memory cleanup for suggestions */
void ranking_free_suggestions(SuggestionItem *suggestions);

#endif /* RANKING_H */

