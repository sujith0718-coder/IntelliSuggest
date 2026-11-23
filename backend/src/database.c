#include "database.h"
#include "db_cloud.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static sqlite3 *g_db = NULL;
static DBDriverType g_driver_type = DB_DRIVER_SQLITE_LOCAL;

DBDriverType db_get_driver_type(void) {
    return g_driver_type;
}

const char* db_get_driver_name(void) {
    return (g_driver_type == DB_DRIVER_SQLITE_CLOUD) ? "SQLiteCloud (Remote)" : "SQLite (Local Embedded)";
}

static bool str_equals_ci(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
        if (ca != cb) return false;
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

static void get_iso_now(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    if (tm) {
        strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", tm);
    } else {
        snprintf(buf, len, "1970-01-01T00:00:00Z");
    }
}

bool db_init(const char *db_path) {
    /* Determine driver type: check DATABASE_DRIVER env var or URI scheme */
    const char *driver_env = getenv("DATABASE_DRIVER");
    bool want_cloud = false;
    if (driver_env && (str_equals_ci(driver_env, "sqlitecloud") || str_equals_ci(driver_env, "cloud"))) {
        want_cloud = true;
    } else if (db_path && strncmp(db_path, "sqlitecloud://", 14) == 0) {
        want_cloud = true;
    }

    if (want_cloud) {
        g_driver_type = DB_DRIVER_SQLITE_CLOUD;
        printf("[Database] Initializing SQLiteCloud remote driver (%s)...\n",
               db_get_driver_name());
        if (!db_cloud_init(db_path)) {
            fprintf(stderr, "[Database Fatal Error] Failed to initialize SQLiteCloud remote driver. Aborting.\n");
            return false;
        }
        return true;
    }

    /* Fallback / Local Driver (default) */
    g_driver_type = DB_DRIVER_SQLITE_LOCAL;
    const char *target_path = (db_path && strlen(db_path) > 0) ? db_path : "intellisuggest.db";
    printf("[Database] Initializing SQLite local embedded driver (%s)...\n", target_path);

    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }

    int rc = sqlite3_open(target_path, &g_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB Error] Cannot open database %s: %s\n", target_path, sqlite3_errmsg(g_db));
        if (g_db) {
            sqlite3_close(g_db);
            g_db = NULL;
        }
        return false;
    }

    /* Enable foreign keys and WAL mode for reliability and performance */
    sqlite3_exec(g_db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL);

    return db_create_schema();
}

void db_close(void) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        db_cloud_close();
        return;
    }
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

bool db_create_schema(void) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_create_schema();
    }
    if (!g_db) return false;

    const char *schema_sql =
        "CREATE TABLE IF NOT EXISTS keywords ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  keyword TEXT UNIQUE NOT NULL,"
        "  frequency INTEGER DEFAULT 0,"
        "  created_at TEXT NOT NULL,"
        "  last_searched_at TEXT,"
        "  is_active INTEGER DEFAULT 1"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_kw_keyword ON keywords(keyword);"
        "CREATE INDEX IF NOT EXISTS idx_kw_frequency ON keywords(frequency DESC);"
        "CREATE INDEX IF NOT EXISTS idx_kw_active ON keywords(is_active);"
        "CREATE TABLE IF NOT EXISTS search_history ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  keyword_id INTEGER NOT NULL,"
        "  searched_at TEXT NOT NULL,"
        "  FOREIGN KEY(keyword_id) REFERENCES keywords(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_sh_keyword ON search_history(keyword_id);"
        "CREATE INDEX IF NOT EXISTS idx_sh_time ON search_history(searched_at DESC);"
        "CREATE TABLE IF NOT EXISTS concepts ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  concept_name TEXT UNIQUE NOT NULL,"
        "  description TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS concept_keywords ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  concept_id INTEGER NOT NULL,"
        "  keyword TEXT NOT NULL,"
        "  FOREIGN KEY(concept_id) REFERENCES concepts(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_ck_concept ON concept_keywords(concept_id);"
        "CREATE TABLE IF NOT EXISTS search_metrics ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  metric_name TEXT NOT NULL,"
        "  metric_value REAL NOT NULL,"
        "  recorded_at TEXT NOT NULL"
        ");";

    char *errmsg = NULL;
    int rc = sqlite3_exec(g_db, schema_sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB Error] Schema creation failed: %s\n", errmsg ? errmsg : "unknown");
        if (errmsg) sqlite3_free(errmsg);
        return false;
    }
    return true;
}

bool db_seed_from_csv(const char *csv_path) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_seed_from_csv(csv_path);
    }
    if (!g_db || !csv_path) return false;

    FILE *f = fopen(csv_path, "r");
    if (!f) return false;

    char line[256];
    char now[32];
    get_iso_now(now, sizeof(now));

    /* Skip header line if present */
    if (fgets(line, sizeof(line), f)) {
        if (strstr(line, "keyword") == NULL) {
            rewind(f);
        }
    }

    sqlite3_exec(g_db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    sqlite3_stmt *stmt = NULL;
    const char *insert_sql = "INSERT OR IGNORE INTO keywords(keyword, frequency, created_at, is_active) VALUES(?, ?, ?, 1);";
    if (sqlite3_prepare_v2(g_db, insert_sql, -1, &stmt, NULL) != SQLITE_OK) {
        fclose(f);
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        return false;
    }

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *comma = strchr(line, ',');
        if (!comma) continue;

        *comma = '\0';
        char *raw_kw = line;
        int freq = atoi(comma + 1);

        char *norm = trie_normalize(raw_kw);
        if (norm && strlen(norm) > 0) {
            sqlite3_bind_text(stmt, 1, norm, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, freq);
            sqlite3_bind_text(stmt, 3, now, -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
            free(norm);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(g_db, "COMMIT;", NULL, NULL, NULL);
    fclose(f);
    return true;
}

/* Hydrate Trie and Hash Table at server startup */
size_t db_load_keywords_into_memory(Trie *trie, HashTable *ht) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_load_keywords_into_memory(trie, ht);
    }
    if (!g_db || !trie || !ht) return 0;

    sqlite3_stmt *stmt = NULL;
    const char *query = "SELECT keyword, frequency, created_at, last_searched_at, is_active FROM keywords WHERE is_active = 1;";
    if (sqlite3_prepare_v2(g_db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    size_t loaded = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *kw = (const char *)sqlite3_column_text(stmt, 0);
        int freq = sqlite3_column_int(stmt, 1);
        const char *created = (const char *)sqlite3_column_text(stmt, 2);
        const char *last_s = (const char *)sqlite3_column_text(stmt, 3);
        bool active = sqlite3_column_int(stmt, 4) == 1;

        if (kw) {
            trie_insert(trie, kw, freq);
            hashtable_insert(ht, kw, freq, created, last_s, active);
            loaded++;
        }
    }

    sqlite3_finalize(stmt);
    return loaded;
}

/* Record a search submission: update frequency, timestamps, and history log */
bool db_record_search(const char *keyword, int *out_new_frequency, const char *searched_at) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_record_search(keyword, out_new_frequency, searched_at);
    }
    if (!g_db || !keyword) return false;

    char *norm = trie_normalize(keyword);
    if (!norm) return false;

    char now[32];
    if (searched_at && strlen(searched_at) > 0) {
        strncpy(now, searched_at, sizeof(now) - 1);
        now[sizeof(now) - 1] = '\0';
    } else {
        get_iso_now(now, sizeof(now));
    }

    sqlite3_exec(g_db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    /* 1. Upsert keyword */
    sqlite3_stmt *stmt1 = NULL;
    const char *upsert_sql =
        "INSERT INTO keywords(keyword, frequency, created_at, last_searched_at, is_active) "
        "VALUES(?, 1, ?, ?, 1) "
        "ON CONFLICT(keyword) DO UPDATE SET "
        "frequency = frequency + 1, "
        "last_searched_at = excluded.last_searched_at, "
        "is_active = 1;";

    if (sqlite3_prepare_v2(g_db, upsert_sql, -1, &stmt1, NULL) != SQLITE_OK) {
        sqlite3_exec(g_db, "ROLLBACK;", NULL, NULL, NULL);
        free(norm);
        return false;
    }

    sqlite3_bind_text(stmt1, 1, norm, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt1, 2, now, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt1, 3, now, -1, SQLITE_STATIC);
    sqlite3_step(stmt1);
    sqlite3_finalize(stmt1);

    /* 2. Retrieve keyword ID and new frequency */
    sqlite3_stmt *stmt2 = NULL;
    const char *sel_sql = "SELECT id, frequency FROM keywords WHERE keyword = ?;";
    int kw_id = 0;
    int new_freq = 1;

    if (sqlite3_prepare_v2(g_db, sel_sql, -1, &stmt2, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt2, 1, norm, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt2) == SQLITE_ROW) {
            kw_id = sqlite3_column_int(stmt2, 0);
            new_freq = sqlite3_column_int(stmt2, 1);
        }
        sqlite3_finalize(stmt2);
    }

    /* 3. Record in search_history */
    if (kw_id > 0) {
        sqlite3_stmt *stmt3 = NULL;
        const char *hist_sql = "INSERT INTO search_history(keyword_id, searched_at) VALUES(?, ?);";
        if (sqlite3_prepare_v2(g_db, hist_sql, -1, &stmt3, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt3, 1, kw_id);
            sqlite3_bind_text(stmt3, 2, now, -1, SQLITE_STATIC);
            sqlite3_step(stmt3);
            sqlite3_finalize(stmt3);
        }
    }

    sqlite3_exec(g_db, "COMMIT;", NULL, NULL, NULL);

    if (out_new_frequency) {
        *out_new_frequency = new_freq;
    }

    free(norm);
    return true;
}

/* Add keyword manually via Admin */
bool db_add_keyword(const char *keyword, int initial_frequency, int *out_id) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_add_keyword(keyword, initial_frequency, out_id);
    }
    if (!g_db || !keyword) return false;

    char *norm = trie_normalize(keyword);
    if (!norm) return false;

    char now[32];
    get_iso_now(now, sizeof(now));

    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO keywords(keyword, frequency, created_at, is_active) VALUES(?, ?, ?, 1);";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(norm);
        return false;
    }

    sqlite3_bind_text(stmt, 1, norm, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, initial_frequency);
    sqlite3_bind_text(stmt, 3, now, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    free(norm);

    if (rc == SQLITE_DONE) {
        if (out_id) {
            *out_id = (int)sqlite3_last_insert_rowid(g_db);
        }
        return true;
    }
    return false;
}

/* Update keyword details via Admin */
bool db_update_keyword(int id, const char *keyword, int frequency, bool is_active) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_update_keyword(id, keyword, frequency, is_active);
    }
    if (!g_db || id <= 0) return false;

    sqlite3_stmt *stmt = NULL;
    const char *sql = "UPDATE keywords SET keyword = ?, frequency = ?, is_active = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    char *norm = keyword ? trie_normalize(keyword) : NULL;
    sqlite3_bind_text(stmt, 1, norm ? norm : keyword, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, frequency);
    sqlite3_bind_int(stmt, 3, is_active ? 1 : 0);
    sqlite3_bind_int(stmt, 4, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (norm) free(norm);

    return (rc == SQLITE_DONE);
}

/* Delete keyword by ID */
bool db_delete_keyword(int id, char *out_deleted_keyword, size_t max_len) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_delete_keyword(id, out_deleted_keyword, max_len);
    }
    if (!g_db || id <= 0) return false;

    /* Get keyword string before deleting */
    if (out_deleted_keyword && max_len > 0) {
        out_deleted_keyword[0] = '\0';
        sqlite3_stmt *sel = NULL;
        if (sqlite3_prepare_v2(g_db, "SELECT keyword FROM keywords WHERE id = ?;", -1, &sel, NULL) == SQLITE_OK) {
            sqlite3_bind_int(sel, 1, id);
            if (sqlite3_step(sel) == SQLITE_ROW) {
                const char *kw = (const char *)sqlite3_column_text(sel, 0);
                if (kw) {
                    strncpy(out_deleted_keyword, kw, max_len - 1);
                    out_deleted_keyword[max_len - 1] = '\0';
                }
            }
            sqlite3_finalize(sel);
        }
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, "DELETE FROM keywords WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

/* Cleanup keywords inactive for > days_threshold */
size_t db_cleanup_expired(int days_threshold, char ***out_deleted_keywords) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_cleanup_expired(days_threshold, out_deleted_keywords);
    }
    if (!g_db || days_threshold <= 0) return 0;

    char cutoff[32];
    time_t cutoff_t = time(NULL) - (time_t)(days_threshold * 86400);
    struct tm *tm = gmtime(&cutoff_t);
    if (tm) {
        strftime(cutoff, sizeof(cutoff), "%Y-%m-%dT%H:%M:%SZ", tm);
    } else {
        return 0;
    }

    /* Find expired keywords */
    sqlite3_stmt *sel = NULL;
    const char *sel_sql =
        "SELECT keyword FROM keywords "
        "WHERE (last_searched_at IS NOT NULL AND last_searched_at < ?) "
        "   OR (last_searched_at IS NULL AND created_at < ?);";

    if (sqlite3_prepare_v2(g_db, sel_sql, -1, &sel, NULL) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(sel, 1, cutoff, -1, SQLITE_STATIC);
    sqlite3_bind_text(sel, 2, cutoff, -1, SQLITE_STATIC);

    char **deleted_list = NULL;
    size_t count = 0;
    size_t cap = 16;
    if (out_deleted_keywords) {
        deleted_list = (char **)malloc(cap * sizeof(char *));
    }

    while (sqlite3_step(sel) == SQLITE_ROW) {
        const char *kw = (const char *)sqlite3_column_text(sel, 0);
        if (kw && out_deleted_keywords) {
            if (count >= cap) {
                cap *= 2;
                deleted_list = (char **)realloc(deleted_list, cap * sizeof(char *));
            }
            deleted_list[count++] = strdup(kw);
        }
    }
    sqlite3_finalize(sel);

    /* Delete them */
    sqlite3_stmt *del = NULL;
    const char *del_sql =
        "DELETE FROM keywords "
        "WHERE (last_searched_at IS NOT NULL AND last_searched_at < ?) "
        "   OR (last_searched_at IS NULL AND created_at < ?);";

    if (sqlite3_prepare_v2(g_db, del_sql, -1, &del, NULL) == SQLITE_OK) {
        sqlite3_bind_text(del, 1, cutoff, -1, SQLITE_STATIC);
        sqlite3_bind_text(del, 2, cutoff, -1, SQLITE_STATIC);
        sqlite3_step(del);
        sqlite3_finalize(del);
    }

    if (out_deleted_keywords) {
        *out_deleted_keywords = deleted_list;
    }
    return count;
}

/* Get recent searches */
size_t db_get_recent_history(size_t limit, DBHistoryRecord **out_records) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_get_recent_history(limit, out_records);
    }
    if (!g_db || !out_records || limit == 0) {
        if (out_records) *out_records = NULL;
        return 0;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT h.id, k.keyword, h.searched_at "
        "FROM search_history h "
        "JOIN keywords k ON h.keyword_id = k.id "
        "ORDER BY h.id DESC LIMIT ?;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out_records = NULL;
        return 0;
    }

    sqlite3_bind_int(stmt, 1, (int)limit);

    DBHistoryRecord *records = (DBHistoryRecord *)malloc(limit * sizeof(DBHistoryRecord));
    size_t count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && count < limit) {
        records[count].id = sqlite3_column_int(stmt, 0);
        const char *kw = (const char *)sqlite3_column_text(stmt, 1);
        const char *time_str = (const char *)sqlite3_column_text(stmt, 2);

        strncpy(records[count].keyword, kw ? kw : "", sizeof(records[count].keyword) - 1);
        records[count].keyword[sizeof(records[count].keyword) - 1] = '\0';

        strncpy(records[count].searched_at, time_str ? time_str : "", sizeof(records[count].searched_at) - 1);
        records[count].searched_at[sizeof(records[count].searched_at) - 1] = '\0';

        count++;
    }

    sqlite3_finalize(stmt);
    *out_records = records;
    return count;
}

void db_free_history(DBHistoryRecord *records) {
    if (records) free(records);
}

/* Get overall popular keywords */
size_t db_get_popular_keywords(size_t limit, DBKeywordRecord **out_records) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_get_popular_keywords(limit, out_records);
    }
    if (!g_db || !out_records || limit == 0) {
        if (out_records) *out_records = NULL;
        return 0;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id, keyword, frequency, created_at, last_searched_at, is_active "
        "FROM keywords WHERE is_active = 1 "
        "ORDER BY frequency DESC, keyword ASC LIMIT ?;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out_records = NULL;
        return 0;
    }

    sqlite3_bind_int(stmt, 1, (int)limit);

    DBKeywordRecord *records = (DBKeywordRecord *)malloc(limit * sizeof(DBKeywordRecord));
    size_t count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && count < limit) {
        records[count].id = sqlite3_column_int(stmt, 0);
        const char *kw = (const char *)sqlite3_column_text(stmt, 1);
        records[count].frequency = sqlite3_column_int(stmt, 2);
        const char *created = (const char *)sqlite3_column_text(stmt, 3);
        const char *last_s = (const char *)sqlite3_column_text(stmt, 4);
        records[count].is_active = sqlite3_column_int(stmt, 5) == 1;

        strncpy(records[count].keyword, kw ? kw : "", sizeof(records[count].keyword) - 1);
        records[count].keyword[sizeof(records[count].keyword) - 1] = '\0';

        strncpy(records[count].created_at, created ? created : "", sizeof(records[count].created_at) - 1);
        records[count].created_at[sizeof(records[count].created_at) - 1] = '\0';

        strncpy(records[count].last_searched_at, last_s ? last_s : "", sizeof(records[count].last_searched_at) - 1);
        records[count].last_searched_at[sizeof(records[count].last_searched_at) - 1] = '\0';

        count++;
    }

    sqlite3_finalize(stmt);
    *out_records = records;
    return count;
}

/* Get all keywords */
size_t db_get_all_keywords(DBKeywordRecord **out_records) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_get_all_keywords(out_records);
    }
    if (!g_db || !out_records) {
        if (out_records) *out_records = NULL;
        return 0;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, keyword, frequency, created_at, last_searched_at, is_active FROM keywords ORDER BY id ASC;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out_records = NULL;
        return 0;
    }

    size_t cap = 64;
    size_t count = 0;
    DBKeywordRecord *records = (DBKeywordRecord *)malloc(cap * sizeof(DBKeywordRecord));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            cap *= 2;
            records = (DBKeywordRecord *)realloc(records, cap * sizeof(DBKeywordRecord));
        }

        records[count].id = sqlite3_column_int(stmt, 0);
        const char *kw = (const char *)sqlite3_column_text(stmt, 1);
        records[count].frequency = sqlite3_column_int(stmt, 2);
        const char *created = (const char *)sqlite3_column_text(stmt, 3);
        const char *last_s = (const char *)sqlite3_column_text(stmt, 4);
        records[count].is_active = sqlite3_column_int(stmt, 5) == 1;

        strncpy(records[count].keyword, kw ? kw : "", sizeof(records[count].keyword) - 1);
        records[count].keyword[sizeof(records[count].keyword) - 1] = '\0';

        strncpy(records[count].created_at, created ? created : "", sizeof(records[count].created_at) - 1);
        records[count].created_at[sizeof(records[count].created_at) - 1] = '\0';

        strncpy(records[count].last_searched_at, last_s ? last_s : "", sizeof(records[count].last_searched_at) - 1);
        records[count].last_searched_at[sizeof(records[count].last_searched_at) - 1] = '\0';

        count++;
    }

    sqlite3_finalize(stmt);
    *out_records = records;
    return count;
}

void db_free_keywords(DBKeywordRecord *records) {
    if (records) free(records);
}

/* Analytics Summary */
bool db_get_analytics_summary(DBAnalyticsSummary *out_summary) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_get_analytics_summary(out_summary);
    }
    if (!g_db || !out_summary) return false;

    memset(out_summary, 0, sizeof(DBAnalyticsSummary));

    sqlite3_stmt *s1 = NULL;
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*), SUM(CASE WHEN is_active=1 THEN 1 ELSE 0 END) FROM keywords;", -1, &s1, NULL) == SQLITE_OK) {
        if (sqlite3_step(s1) == SQLITE_ROW) {
            out_summary->total_keywords = (size_t)sqlite3_column_int(s1, 0);
            out_summary->active_keywords = (size_t)sqlite3_column_int(s1, 1);
        }
        sqlite3_finalize(s1);
    }

    sqlite3_stmt *s2 = NULL;
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM search_history;", -1, &s2, NULL) == SQLITE_OK) {
        if (sqlite3_step(s2) == SQLITE_ROW) {
            out_summary->total_searches = (size_t)sqlite3_column_int(s2, 0);
        }
        sqlite3_finalize(s2);
    }

    return true;
}

