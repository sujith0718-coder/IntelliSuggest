/**
 * ============================================================================
 * INTELLISUGGEST - SQLITECLOUD CLOUD INTEGRATION TEST SUITE
 * ----------------------------------------------------------------------------
 * Rigorous end-to-end integration tests verifying real remote database
 * operations against SQLiteCloud on `intellisuggest.sqlite`.
 * ============================================================================
 */

#include "database.h"
#include "db_cloud.h"
#include "trie.h"
#include "hashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_PASS(desc) printf("  [PASS] %s\n", desc)
#define TEST_FAIL(desc) do { printf("  [FAIL] %s\n", desc); exit(1); } while(0)
#define ASSERT_TRUE(cond, desc) do { if (cond) TEST_PASS(desc); else TEST_FAIL(desc); } while(0)

int main(void) {
    printf("====================================================\n");
    printf("   INTELLISUGGEST - SQLITECLOUD CLOUD TEST SUITE    \n");
    printf("====================================================\n\n");

    const char *cloud_url = getenv("SQLITECLOUD_URL");
    if (!cloud_url || strlen(cloud_url) == 0) {
        printf("Notice: SQLITECLOUD_URL environment variable is not set.\n");
        printf("Skipping remote cloud live test suite.\n");
        return 0;
    }

    /* Configure driver */
    putenv("DATABASE_DRIVER=sqlitecloud");

    printf("--- Test Suite 1: SQLiteCloud Connection & Schema ---\n");
    bool init_ok = db_init(cloud_url);
    ASSERT_TRUE(init_ok, "db_init successfully connected to SQLiteCloud");
    ASSERT_TRUE(db_get_driver_type() == DB_DRIVER_SQLITE_CLOUD, "Active driver is DB_DRIVER_SQLITE_CLOUD");
    printf("  Driver name: %s\n", db_get_driver_name());

    bool schema_ok = db_create_schema();
    ASSERT_TRUE(schema_ok, "db_create_schema verified all 5 tables in intellisuggest.sqlite");

    printf("\n--- Test Suite 2: Seeding & In-Memory Hydration ---\n");
    bool seed_ok = db_seed_from_csv("database/sample_data.csv");
    ASSERT_TRUE(seed_ok, "db_seed_from_csv populated CS keywords into SQLiteCloud");

    Trie *trie = trie_create();
    HashTable *ht = hashtable_create(64);
    size_t loaded = db_load_keywords_into_memory(trie, ht);
    ASSERT_TRUE(loaded >= 10, "db_load_keywords_into_memory hydrated >= 10 keywords from cloud");
    printf("  Hydrated %zu keywords from SQLiteCloud into in-memory Trie and Hash Table\n", loaded);

    int py_freq = 0;
    ASSERT_TRUE(trie_search_exact(trie, "python", &py_freq), "Trie contains 'python' loaded from cloud");
    ASSERT_TRUE(hashtable_search(ht, "dijkstra algorithm") != NULL, "Hash table contains 'dijkstra algorithm'");

    printf("\n--- Test Suite 3: Remote Search Recording & Frequency Updates ---\n");
    int new_freq1 = 0;
    bool rec1 = db_record_search("cloud integration test", &new_freq1, NULL);
    ASSERT_TRUE(rec1, "Recorded first search for 'cloud integration test' in cloud");
    ASSERT_TRUE(new_freq1 >= 1, "Frequency is >= 1");

    int new_freq2 = 0;
    bool rec2 = db_record_search("cloud integration test", &new_freq2, NULL);
    ASSERT_TRUE(rec2, "Recorded second search for 'cloud integration test' in cloud");
    ASSERT_TRUE(new_freq2 == new_freq1 + 1, "Frequency atomically incremented by 1 in cloud");

    printf("\n--- Test Suite 4: History & Popular Keywords Retrieval ---\n");
    DBHistoryRecord *hist = NULL;
    size_t hist_count = db_get_recent_history(5, &hist);
    ASSERT_TRUE(hist_count > 0 && hist != NULL, "Retrieved recent search history from cloud");
    printf("  Most recent search in cloud: '%s' at %s\n", hist[0].keyword, hist[0].searched_at);
    db_free_history(hist);

    DBKeywordRecord *pop = NULL;
    size_t pop_count = db_get_popular_keywords(5, &pop);
    ASSERT_TRUE(pop_count > 0 && pop != NULL, "Retrieved popular keywords ranking from cloud");
    printf("  Rank 1 keyword in cloud: '%s' (freq: %d)\n", pop[0].keyword, pop[0].frequency);
    db_free_keywords(pop);

    printf("\n--- Test Suite 5: Admin Operations & Expiration ---\n");
    int added_id = 0;
    bool add_ok = db_add_keyword("temporary test keyword", 77, &added_id);
    ASSERT_TRUE(add_ok, "Admin added keyword to SQLiteCloud");
    ASSERT_TRUE(added_id > 0, "Keyword assigned valid ID");

    bool upd_ok = db_update_keyword(added_id, "temporary test keyword", 99, true);
    ASSERT_TRUE(upd_ok, "Updated keyword frequency to 99 in cloud");

    char deleted_kw[128];
    bool del_ok = db_delete_keyword(added_id, deleted_kw, sizeof(deleted_kw));
    ASSERT_TRUE(del_ok, "Deleted test keyword from cloud");

    DBAnalyticsSummary summary;
    bool sum_ok = db_get_analytics_summary(&summary);
    ASSERT_TRUE(sum_ok, "Retrieved analytics summary from cloud");
    printf("  Cloud DB Summary: Total Keywords=%zu, Active=%zu, Searches=%zu\n",
           summary.total_keywords, summary.active_keywords, summary.total_searches);

    trie_free(trie);
    hashtable_free(ht);
    db_close();

    printf("\n====================================================\n");
    printf("All SQLiteCloud Remote Integration Tests Passed 100%%!\n");
    printf("====================================================\n");
    return 0;
}
