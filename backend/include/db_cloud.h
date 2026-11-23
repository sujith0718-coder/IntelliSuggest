#ifndef DB_CLOUD_H
#define DB_CLOUD_H

#include "database.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Configure and initialize SQLiteCloud remote driver.
 *
 * @param sqlitecloud_url URL or connection string (e.g. "sqlitecloud://host:8860/db?apikey=...")
 * @return true if remote connection succeeded and verified, false on failure.
 */
bool db_cloud_init(const char *sqlitecloud_url);

/**
 * Close SQLiteCloud driver and free resources.
 */
void db_cloud_close(void);

/**
 * Create application schema tables in SQLiteCloud.
 */
bool db_cloud_create_schema(void);

/**
 * Seed initial dataset into SQLiteCloud if keywords table is empty.
 */
bool db_cloud_seed_from_csv(const char *csv_path);

/**
 * Hydrate in-memory Trie and Hash Table from SQLiteCloud keywords.
 */
size_t db_cloud_load_keywords_into_memory(Trie *trie, HashTable *ht);

/**
 * Record a search event and update frequency in SQLiteCloud.
 */
bool db_cloud_record_search(const char *keyword, int *out_new_frequency, const char *searched_at);

/**
 * Add a new keyword in SQLiteCloud.
 */
bool db_cloud_add_keyword(const char *keyword, int initial_frequency, int *out_id);

/**
 * Update an existing keyword in SQLiteCloud.
 */
bool db_cloud_update_keyword(int id, const char *keyword, int frequency, bool is_active);

/**
 * Delete a keyword by ID in SQLiteCloud.
 */
bool db_cloud_delete_keyword(int id, char *out_deleted_keyword, size_t max_len);

/**
 * Cleanup expired keywords in SQLiteCloud.
 */
size_t db_cloud_cleanup_expired(int days_threshold, char ***out_deleted_keywords);

/**
 * Query recent search history from SQLiteCloud.
 */
size_t db_cloud_get_recent_history(size_t limit, DBHistoryRecord **out_records);

/**
 * Query top popular keywords from SQLiteCloud.
 */
size_t db_cloud_get_popular_keywords(size_t limit, DBKeywordRecord **out_records);

/**
 * Query all keywords from SQLiteCloud.
 */
size_t db_cloud_get_all_keywords(DBKeywordRecord **out_records);

/**
 * Query analytics summary from SQLiteCloud.
 */
bool db_cloud_get_analytics_summary(DBAnalyticsSummary *out_summary);

/**
 * Query trending and expired keywords from SQLiteCloud.
 */
size_t db_cloud_get_trending_keywords(size_t limit, int hours_window, DBKeywordRecord **out_records);
size_t db_cloud_get_expired_keywords(int days_threshold, DBKeywordRecord **out_records);

/**
 * Concept management in SQLiteCloud.
 */
size_t db_cloud_get_all_concepts(DBConceptRecord **out_records);
bool db_cloud_get_concept_by_id(int id, DBConceptRecord *out_record);
bool db_cloud_add_concept(const char *name, const char *description, const char *keywords, int *out_id);
bool db_cloud_update_concept(int id, const char *name, const char *description, const char *keywords);
bool db_cloud_delete_concept(int id);
size_t db_cloud_load_concepts_into_memory(ConceptIndex *idx);

#ifdef __cplusplus
}
#endif

#endif /* DB_CLOUD_H */

