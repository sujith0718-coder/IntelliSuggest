#ifndef DATABASE_H
#define DATABASE_H

#include "trie.h"
#include "hashmap.h"
#include <stdbool.h>
#include <stddef.h>

/* History record */
typedef struct {
    int id;
    char keyword[128];
    char searched_at[32];
} DBHistoryRecord;

/* Keyword record */
typedef struct {
    int id;
    char keyword[128];
    int frequency;
    char created_at[32];
    char last_searched_at[32];
    bool is_active;
} DBKeywordRecord;

/* Analytics Summary */
typedef struct {
    size_t total_keywords;
    size_t active_keywords;
    size_t total_searches;
    size_t expired_keywords;
} DBAnalyticsSummary;

/* Lifecycle */
bool db_init(const char *db_path);
void db_close(void);
bool db_create_schema(void);
bool db_seed_from_csv(const char *csv_path);

/* Startup Hydration */
size_t db_load_keywords_into_memory(Trie *trie, HashTable *ht);

/* Search Recording */
bool db_record_search(const char *keyword, int *out_new_frequency, const char *searched_at);

/* Keyword Administration */
bool db_add_keyword(const char *keyword, int initial_frequency, int *out_id);
bool db_update_keyword(int id, const char *keyword, int frequency, bool is_active);
bool db_delete_keyword(int id, char *out_deleted_keyword, size_t max_len);
size_t db_cleanup_expired(int days_threshold, char ***out_deleted_keywords);

/* Queries */
size_t db_get_recent_history(size_t limit, DBHistoryRecord **out_records);
void db_free_history(DBHistoryRecord *records);

size_t db_get_popular_keywords(size_t limit, DBKeywordRecord **out_records);
size_t db_get_all_keywords(DBKeywordRecord **out_records);
void db_free_keywords(DBKeywordRecord *records);

bool db_get_analytics_summary(DBAnalyticsSummary *out_summary);

#endif /* DATABASE_H */

