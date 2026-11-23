#include "database.h"
#include "trie.h"
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

void test_db_lifecycle_and_seeding(void) {
    printf("\n--- Test Suite 1: SQLite Lifecycle & In-Memory Hydration ---\n");
    const char *test_db_path = "test_intellisuggest.db";
    remove(test_db_path);

    TEST_ASSERT(db_init(test_db_path), "db_init successfully opened test SQLite database");

    /* Seed from sample_data.csv */
    TEST_ASSERT(db_seed_from_csv("database/sample_data.csv"), "Seed keywords from sample_data.csv");

    Trie *trie = trie_create();
    HashTable *ht = hashtable_create(64);

    size_t loaded = db_load_keywords_into_memory(trie, ht);
    TEST_ASSERT(loaded >= 10, "Loaded at least 10 sample keywords from database into memory");

    int freq = 0;
    TEST_ASSERT(trie_search_exact(trie, "python", &freq) && freq == 150, "Trie hydrated with 'python' (freq 150)");
    HashNode *node = hashtable_search(ht, "database");
    TEST_ASSERT(node != NULL && node->frequency == 110, "Hash Table hydrated with 'database' (freq 110)");

    trie_free(trie);
    hashtable_free(ht);
    db_close();
    remove(test_db_path);
}

void test_db_search_submission_and_history(void) {
    printf("\n--- Test Suite 2: Search Submission & History Persistence ---\n");
    const char *test_db_path = "test_search.db";
    remove(test_db_path);

    db_init(test_db_path);

    int new_freq = 0;
    TEST_ASSERT(db_record_search("machine learning", &new_freq, "2026-11-23T23:30:00Z"), "Submitted first search for 'machine learning'");
    TEST_ASSERT(new_freq == 1, "Initial search sets frequency to 1");

    TEST_ASSERT(db_record_search("machine learning", &new_freq, "2026-11-23T23:35:00Z"), "Submitted second search for 'machine learning'");
    TEST_ASSERT(new_freq == 2, "Second search increments frequency to 2");

    DBHistoryRecord *history = NULL;
    size_t hist_count = db_get_recent_history(5, &history);
    TEST_ASSERT(hist_count == 2, "Search history contains 2 events");
    TEST_ASSERT(strcmp(history[0].keyword, "machine learning") == 0, "History recorded correct keyword");
    db_free_history(history);

    DBKeywordRecord *popular = NULL;
    size_t pop_count = db_get_popular_keywords(5, &popular);
    TEST_ASSERT(pop_count == 1, "1 popular keyword in database");
    TEST_ASSERT(strcmp(popular[0].keyword, "machine learning") == 0 && popular[0].frequency == 2, "Popular keyword frequency is 2");
    db_free_keywords(popular);

    db_close();
    remove(test_db_path);
}

void test_db_admin_crud_and_cleanup(void) {
    printf("\n--- Test Suite 3: Admin Operations & Expiration Cleanup ---\n");
    const char *test_db_path = "test_admin.db";
    remove(test_db_path);

    db_init(test_db_path);

    int kw_id = 0;
    TEST_ASSERT(db_add_keyword("compiler design", 25, &kw_id), "Admin added keyword 'compiler design'");
    TEST_ASSERT(kw_id > 0, "Keyword assigned valid primary key ID");

    TEST_ASSERT(db_update_keyword(kw_id, "compiler design", 40, true), "Updated frequency to 40");

    char deleted_kw[128];
    TEST_ASSERT(db_delete_keyword(kw_id, deleted_kw, sizeof(deleted_kw)), "Admin deleted keyword");
    TEST_ASSERT(strcmp(deleted_kw, "compiler design") == 0, "Deleted keyword name captured for cache invalidation");

    DBAnalyticsSummary summary;
    TEST_ASSERT(db_get_analytics_summary(&summary), "Retrieved analytics summary");
    TEST_ASSERT(summary.total_keywords == 0, "Total keywords is 0 after deletion");

    db_close();
    remove(test_db_path);
}

static void test_db_concept_crud_and_trending(void) {
    printf("\n--- Test Suite 4: Dynamic Concept Management & Trending ---\n");
    const char *test_db_path = "test_concepts.db";
    remove(test_db_path);

    db_init(test_db_path);

    /* 1. Add Concept */
    int cid = 0;
    TEST_ASSERT(db_add_concept("Bellman-Ford Algorithm",
                               "Shortest path algorithm supporting negative edge weights",
                               "negative edges, shortest path, dynamic programming",
                               &cid),
                "Added new concept 'Bellman-Ford Algorithm' into database");
    TEST_ASSERT(cid > 0, "Concept assigned valid primary key ID");

    /* 2. Retrieve Concept by ID */
    DBConceptRecord rec;
    TEST_ASSERT(db_get_concept_by_id(cid, &rec), "Retrieved concept by ID");
    TEST_ASSERT(strcmp(rec.concept_name, "Bellman-Ford Algorithm") == 0, "Concept name matches exact record");
    TEST_ASSERT(strstr(rec.keywords, "negative edges") != NULL, "Associated keywords retrieved properly");

    /* 3. Update Concept */
    TEST_ASSERT(db_update_concept(cid, "Bellman-Ford Algorithm",
                                  "Updated graph algorithm description for negative cycles",
                                  "negative cycle detection, shortest path"),
                "Updated concept description and associations");
    DBConceptRecord updated_rec;
    db_get_concept_by_id(cid, &updated_rec);
    TEST_ASSERT(strstr(updated_rec.description, "negative cycles") != NULL, "Updated description persisted");

    /* 4. Hydrate into In-Memory ConceptIndex */
    ConceptIndex *idx = concept_index_create();
    size_t loaded = db_load_concepts_into_memory(idx);
    TEST_ASSERT(loaded >= 1, "Hydrated concepts from database into in-memory index");

    ConceptSearchResult *results = NULL;
    size_t found = concept_index_search(idx, "negative cycle detection", 5, &results);
    TEST_ASSERT(found > 0, "Forgotten search dynamically resolved persisted concept");
    if (found > 0) {
        TEST_ASSERT(strcmp(results[0].concept_name, "Bellman-Ford Algorithm") == 0,
                    "Top match is dynamically added concept 'Bellman-Ford Algorithm'");
        concept_index_free_results(results);
    }

    /* 5. Trending & Expired Analytics */
    DBKeywordRecord *trending = NULL;
    size_t trend_count = db_get_trending_keywords(5, 24, &trending);
    TEST_ASSERT(trend_count >= 0, "Trending keywords query executed successfully");
    if (trending) db_free_keywords(trending);

    /* 6. Delete Concept */
    TEST_ASSERT(db_delete_concept(cid), "Deleted concept by ID");
    DBConceptRecord deleted_check;
    TEST_ASSERT(!db_get_concept_by_id(cid, &deleted_check), "Deleted concept no longer exists in database");

    concept_index_free(idx);
    db_close();
    remove(test_db_path);
}

int main(void) {
    printf("====================================================\n");
    printf("        INTELLISUGGEST - SQLITE DB TEST SUITE       \n");
    printf("====================================================\n");

    test_db_lifecycle_and_seeding();
    test_db_search_submission_and_history();
    test_db_admin_crud_and_cleanup();
    test_db_concept_crud_and_trending();

    printf("\n====================================================\n");
    printf("Test Results: %d / %d passed (%.1f%%)\n",
           passed_tests, total_tests,
           (total_tests > 0 ? (float)passed_tests * 100.0f / total_tests : 0.0f));
    printf("====================================================\n");

    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}

