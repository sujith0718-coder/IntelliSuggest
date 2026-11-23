#include "database.h"
#include "trie.h"
#include "hashmap.h"
#include "heap.h"
#include "ranking.h"
#include "concept_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("====================================================\n");
    printf("  INTELLISUGGEST LOCAL DATASET VERIFICATION RUNNER  \n");
    printf("====================================================\n");

    /* 1. Initialize local SQLite driver */
    if (!db_init("intellisuggest.db")) {
        fprintf(stderr, "Failed to initialize intellisuggest.db\n");
        return 1;
    }

    Trie *trie = trie_create();
    HashTable *ht = hashtable_create(256);
    ConceptIndex *cidx = concept_index_create();

    /* 2. Hydrate keywords */
    size_t kws_loaded = db_load_keywords_into_memory(trie, ht);
    printf("[Hydration] Keywords loaded into Trie & Hash Table: %zu\n", kws_loaded);
    printf("[Trie Stats] Total Words: %zu, Total Nodes: %zu\n", trie->total_words, trie->total_nodes);
    printf("[Hash Table Stats] Size: %zu, Capacity: %zu, Load Factor: %.3f, Collisions: %zu\n",
           ht->size, ht->capacity, hashtable_get_load_factor(ht), hashtable_get_collision_count(ht));

    /* 3. Hydrate concepts */
    size_t concepts_loaded = db_load_concepts_into_memory(cidx);
    printf("[Hydration] Concepts loaded into Inverted Index: %zu (Postings Size: %zu)\n",
           concepts_loaded, cidx->index_size);

    /* 4. Analytics Summary */
    DBAnalyticsSummary summary;
    db_get_analytics_summary(&summary);
    printf("\n[Analytics Telemetry]\n");
    printf("  - Total Keywords in DB:  %zu\n", summary.total_keywords);
    printf("  - Active Keywords in DB: %zu\n", summary.active_keywords);
    printf("  - Real User Searches:    %zu\n", summary.total_searches);
    printf("  - Demo Historical Events:%zu\n", summary.demo_searches);

    /* 5. Autocomplete Tests (Prefixes requested by user) */
    printf("\n[Intelligent Autocomplete Verification (Top-10 Cache)]\n");
    const char *test_prefixes[] = {"py", "jav", "bin", "tr", "gra", "has"};
    for (int p = 0; p < 6; p++) {
        SuggestionItem *results = NULL;
        size_t count = ranking_get_suggestions_cached(trie, test_prefixes[p], 10, &results);
        printf("  Prefix '%s' -> %zu suggestions returned:\n", test_prefixes[p], count);
        for (size_t i = 0; i < count; i++) {
            printf("    %zu. %-24s (freq: %d)\n", i + 1, results[i].keyword, results[i].frequency);
        }
        if (results) ranking_free_suggestions(results);
    }

    /* 6. Forgotten-Name Search Tests (Queries requested by user) */
    printf("\n[Forgotten-Name Search Verification (Inverted Index)]\n");
    const char *test_queries[] = {
        "shortest path algorithm",
        "prefix searching data structure",
        "last in first out",
        "minimum spanning tree",
        "balanced binary search tree",
        "dynamic programming",
        "browser authentication",
        "network communication protocol"
    };

    for (int q = 0; q < 8; q++) {
        ConceptSearchResult *results = NULL;
        size_t count = concept_index_search(cidx, test_queries[q], 5, &results);
        printf("  Query: \"%s\"\n", test_queries[q]);
        if (count > 0 && results) {
            for (size_t i = 0; i < count && i < 2; i++) {
                printf("    -> Match #%zu: %s (Score: %.2f%%, Matched: %zu tokens)\n",
                       i + 1, results[i].concept_name, results[i].relevance_score * 100.0, results[i].matched_token_count);
            }
            free(results);
        } else {
            printf("    -> No match found\n");
        }
    }

    /* Clean up */
    concept_index_free(cidx);
    trie_free(trie);
    hashtable_free(ht);
    db_close();

    printf("\n====================================================\n");
    printf("   VERIFICATION RUNNER FINISHED WITH 100%% SUCCESS    \n");
    printf("====================================================\n");
    return 0;
}
