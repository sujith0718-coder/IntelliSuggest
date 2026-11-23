#include "database.h"
#include "db_cloud.h"
#include "concept_index.h"
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
        "  is_demo INTEGER DEFAULT 0,"
        "  FOREIGN KEY(keyword_id) REFERENCES keywords(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_sh_keyword ON search_history(keyword_id);"
        "CREATE INDEX IF NOT EXISTS idx_sh_time ON search_history(searched_at DESC);"
        "CREATE INDEX IF NOT EXISTS idx_sh_demo ON search_history(is_demo);"
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

    /* Auto-migrate existing database to add is_demo if missing */
    sqlite3_exec(g_db, "ALTER TABLE search_history ADD COLUMN is_demo INTEGER DEFAULT 0;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "CREATE INDEX IF NOT EXISTS idx_sh_demo ON search_history(is_demo);", NULL, NULL, NULL);

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
        const char *hist_sql = "INSERT INTO search_history(keyword_id, searched_at, is_demo) VALUES(?, ?, 0);";
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
        "SELECT h.id, k.keyword, h.searched_at, COALESCE(h.is_demo, 0) "
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
        records[count].is_demo = (sqlite3_column_int(stmt, 3) == 1);

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
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*), SUM(CASE WHEN is_active=1 THEN 1 ELSE 0 END), SUM(CASE WHEN is_active=0 THEN 1 ELSE 0 END) FROM keywords;", -1, &s1, NULL) == SQLITE_OK) {
        if (sqlite3_step(s1) == SQLITE_ROW) {
            out_summary->total_keywords = (size_t)sqlite3_column_int(s1, 0);
            out_summary->active_keywords = (size_t)sqlite3_column_int(s1, 1);
            out_summary->expired_keywords = (size_t)sqlite3_column_int(s1, 2);
        }
        sqlite3_finalize(s1);
    }

    sqlite3_stmt *s2 = NULL;
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(CASE WHEN is_demo = 0 THEN 1 END), COUNT(CASE WHEN is_demo = 1 THEN 1 END) FROM search_history;", -1, &s2, NULL) == SQLITE_OK) {
        if (sqlite3_step(s2) == SQLITE_ROW) {
            out_summary->total_searches = (size_t)sqlite3_column_int(s2, 0);
            out_summary->demo_searches = (size_t)sqlite3_column_int(s2, 1);
        }
        sqlite3_finalize(s2);
    }

    return true;
}

/* Trending Keywords */
size_t db_get_trending_keywords(size_t limit, int hours_window, DBKeywordRecord **out_records) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_get_trending_keywords(limit, hours_window, out_records);
    }
    if (!g_db || !out_records || limit == 0) {
        if (out_records) *out_records = NULL;
        return 0;
    }

    if (hours_window <= 0) hours_window = 24;
    time_t cutoff_t = time(NULL) - (time_t)(hours_window * 3600);
    char cutoff_str[32];
    struct tm *tm = gmtime(&cutoff_t);
    if (tm) strftime(cutoff_str, sizeof(cutoff_str), "%Y-%m-%dT%H:%M:%SZ", tm);
    else snprintf(cutoff_str, sizeof(cutoff_str), "1970-01-01T00:00:00Z");

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id, keyword, frequency, created_at, last_searched_at, is_active "
        "FROM keywords WHERE is_active = 1 AND last_searched_at >= ? "
        "ORDER BY frequency DESC LIMIT ?;";

    size_t count = 0;
    DBKeywordRecord *records = (DBKeywordRecord *)malloc(limit * sizeof(DBKeywordRecord));

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, cutoff_str, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, (int)limit);

        while (sqlite3_step(stmt) == SQLITE_ROW && count < limit) {
            records[count].id = sqlite3_column_int(stmt, 0);
            const char *kw = (const char *)sqlite3_column_text(stmt, 1);
            records[count].frequency = sqlite3_column_int(stmt, 2);
            const char *cr = (const char *)sqlite3_column_text(stmt, 3);
            const char *ls = (const char *)sqlite3_column_text(stmt, 4);
            records[count].is_active = (sqlite3_column_int(stmt, 5) == 1);

            strncpy(records[count].keyword, kw ? kw : "", sizeof(records[count].keyword) - 1);
            records[count].keyword[sizeof(records[count].keyword) - 1] = '\0';
            strncpy(records[count].created_at, cr ? cr : "", sizeof(records[count].created_at) - 1);
            records[count].created_at[sizeof(records[count].created_at) - 1] = '\0';
            strncpy(records[count].last_searched_at, ls ? ls : "", sizeof(records[count].last_searched_at) - 1);
            records[count].last_searched_at[sizeof(records[count].last_searched_at) - 1] = '\0';
            count++;
        }
        sqlite3_finalize(stmt);
    }

    /* Fallback to top active keywords if few results in recent window */
    if (count < limit) {
        sqlite3_stmt *fb = NULL;
        const char *fb_sql =
            "SELECT id, keyword, frequency, created_at, last_searched_at, is_active "
            "FROM keywords WHERE is_active = 1 ORDER BY frequency DESC LIMIT ?;";
        if (sqlite3_prepare_v2(g_db, fb_sql, -1, &fb, NULL) == SQLITE_OK) {
            sqlite3_bind_int(fb, 1, (int)limit);
            while (sqlite3_step(fb) == SQLITE_ROW && count < limit) {
                int kid = sqlite3_column_int(fb, 0);
                bool already = false;
                for (size_t i = 0; i < count; i++) {
                    if (records[i].id == kid) { already = true; break; }
                }
                if (!already) {
                    records[count].id = kid;
                    const char *kw = (const char *)sqlite3_column_text(fb, 1);
                    records[count].frequency = sqlite3_column_int(fb, 2);
                    const char *cr = (const char *)sqlite3_column_text(fb, 3);
                    const char *ls = (const char *)sqlite3_column_text(fb, 4);
                    records[count].is_active = (sqlite3_column_int(fb, 5) == 1);

                    strncpy(records[count].keyword, kw ? kw : "", sizeof(records[count].keyword) - 1);
                    records[count].keyword[sizeof(records[count].keyword) - 1] = '\0';
                    strncpy(records[count].created_at, cr ? cr : "", sizeof(records[count].created_at) - 1);
                    records[count].created_at[sizeof(records[count].created_at) - 1] = '\0';
                    strncpy(records[count].last_searched_at, ls ? ls : "", sizeof(records[count].last_searched_at) - 1);
                    records[count].last_searched_at[sizeof(records[count].last_searched_at) - 1] = '\0';
                    count++;
                }
            }
            sqlite3_finalize(fb);
        }
    }

    *out_records = records;
    return count;
}

/* Expired Keywords */
size_t db_get_expired_keywords(int days_threshold, DBKeywordRecord **out_records) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_get_expired_keywords(days_threshold, out_records);
    }
    if (!g_db || !out_records) {
        if (out_records) *out_records = NULL;
        return 0;
    }

    if (days_threshold <= 0) days_threshold = 90;
    time_t cutoff_t = time(NULL) - (time_t)(days_threshold * 86400);
    char cutoff_str[32];
    struct tm *tm = gmtime(&cutoff_t);
    if (tm) strftime(cutoff_str, sizeof(cutoff_str), "%Y-%m-%dT%H:%M:%SZ", tm);
    else snprintf(cutoff_str, sizeof(cutoff_str), "1970-01-01T00:00:00Z");

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id, keyword, frequency, created_at, last_searched_at, is_active "
        "FROM keywords "
        "WHERE is_active = 0 OR ("
        "(last_searched_at IS NOT NULL AND last_searched_at < ?) OR "
        "(last_searched_at IS NULL AND created_at < ?)) "
        "ORDER BY id DESC LIMIT 50;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out_records = NULL;
        return 0;
    }

    sqlite3_bind_text(stmt, 1, cutoff_str, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, cutoff_str, -1, SQLITE_STATIC);

    size_t cap = 16;
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
        const char *cr = (const char *)sqlite3_column_text(stmt, 3);
        const char *ls = (const char *)sqlite3_column_text(stmt, 4);
        records[count].is_active = (sqlite3_column_int(stmt, 5) == 1);

        strncpy(records[count].keyword, kw ? kw : "", sizeof(records[count].keyword) - 1);
        records[count].keyword[sizeof(records[count].keyword) - 1] = '\0';
        strncpy(records[count].created_at, cr ? cr : "", sizeof(records[count].created_at) - 1);
        records[count].created_at[sizeof(records[count].created_at) - 1] = '\0';
        strncpy(records[count].last_searched_at, ls ? ls : "", sizeof(records[count].last_searched_at) - 1);
        records[count].last_searched_at[sizeof(records[count].last_searched_at) - 1] = '\0';
        count++;
    }

    sqlite3_finalize(stmt);
    *out_records = records;
    return count;
}

/* Concept Management */
size_t db_get_all_concepts(DBConceptRecord **out_records) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_get_all_concepts(out_records);
    }
    if (!g_db || !out_records) {
        if (out_records) *out_records = NULL;
        return 0;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT c.id, c.concept_name, c.description, COALESCE(GROUP_CONCAT(ck.keyword, ', '), '') "
        "FROM concepts c LEFT JOIN concept_keywords ck ON c.id = ck.concept_id "
        "GROUP BY c.id ORDER BY c.id ASC;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out_records = NULL;
        return 0;
    }

    size_t cap = 16;
    size_t count = 0;
    DBConceptRecord *records = (DBConceptRecord *)malloc(cap * sizeof(DBConceptRecord));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            cap *= 2;
            records = (DBConceptRecord *)realloc(records, cap * sizeof(DBConceptRecord));
        }

        records[count].id = sqlite3_column_int(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        const char *desc = (const char *)sqlite3_column_text(stmt, 2);
        const char *kws = (const char *)sqlite3_column_text(stmt, 3);

        strncpy(records[count].concept_name, name ? name : "", sizeof(records[count].concept_name) - 1);
        records[count].concept_name[sizeof(records[count].concept_name) - 1] = '\0';
        strncpy(records[count].description, desc ? desc : "", sizeof(records[count].description) - 1);
        records[count].description[sizeof(records[count].description) - 1] = '\0';
        strncpy(records[count].keywords, kws ? kws : "", sizeof(records[count].keywords) - 1);
        records[count].keywords[sizeof(records[count].keywords) - 1] = '\0';

        count++;
    }

    sqlite3_finalize(stmt);
    *out_records = records;
    return count;
}

bool db_get_concept_by_id(int id, DBConceptRecord *out_record) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_get_concept_by_id(id, out_record);
    }
    if (!g_db || !out_record || id <= 0) return false;

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT c.id, c.concept_name, c.description, COALESCE(GROUP_CONCAT(ck.keyword, ', '), '') "
        "FROM concepts c LEFT JOIN concept_keywords ck ON c.id = ck.concept_id "
        "WHERE c.id = ? GROUP BY c.id;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_record->id = sqlite3_column_int(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        const char *desc = (const char *)sqlite3_column_text(stmt, 2);
        const char *kws = (const char *)sqlite3_column_text(stmt, 3);

        strncpy(out_record->concept_name, name ? name : "", sizeof(out_record->concept_name) - 1);
        out_record->concept_name[sizeof(out_record->concept_name) - 1] = '\0';
        strncpy(out_record->description, desc ? desc : "", sizeof(out_record->description) - 1);
        out_record->description[sizeof(out_record->description) - 1] = '\0';
        strncpy(out_record->keywords, kws ? kws : "", sizeof(out_record->keywords) - 1);
        out_record->keywords[sizeof(out_record->keywords) - 1] = '\0';
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool db_add_concept(const char *name, const char *description, const char *keywords, int *out_id) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_add_concept(name, description, keywords, out_id);
    }
    if (!g_db || !name || !description) return false;

    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO concepts (concept_name, description) VALUES (?, ?) ON CONFLICT(concept_name) DO UPDATE SET description = excluded.description;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, description, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return false;

    int concept_id = (int)sqlite3_last_insert_rowid(g_db);
    if (concept_id <= 0) {
        sqlite3_stmt *q = NULL;
        if (sqlite3_prepare_v2(g_db, "SELECT id FROM concepts WHERE concept_name = ?;", -1, &q, NULL) == SQLITE_OK) {
            sqlite3_bind_text(q, 1, name, -1, SQLITE_STATIC);
            if (sqlite3_step(q) == SQLITE_ROW) concept_id = sqlite3_column_int(q, 0);
            sqlite3_finalize(q);
        }
    }
    if (out_id) *out_id = concept_id;

    /* Associate keywords */
    if (concept_id > 0 && keywords && strlen(keywords) > 0) {
        /* Clear old associations */
        sqlite3_stmt *del = NULL;
        if (sqlite3_prepare_v2(g_db, "DELETE FROM concept_keywords WHERE concept_id = ?;", -1, &del, NULL) == SQLITE_OK) {
            sqlite3_bind_int(del, 1, concept_id);
            sqlite3_step(del);
            sqlite3_finalize(del);
        }

        char buf[512];
        strncpy(buf, keywords, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *saveptr = NULL;
        char *token = strtok_r(buf, ",;", &saveptr);
        while (token) {
            while (*token == ' ') token++;
            char *end = token + strlen(token) - 1;
            while (end > token && *end == ' ') { *end = '\0'; end--; }
            if (strlen(token) > 0) {
                sqlite3_stmt *ins = NULL;
                if (sqlite3_prepare_v2(g_db, "INSERT INTO concept_keywords (concept_id, keyword) VALUES (?, ?);", -1, &ins, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(ins, 1, concept_id);
                    sqlite3_bind_text(ins, 2, token, -1, SQLITE_STATIC);
                    sqlite3_step(ins);
                    sqlite3_finalize(ins);
                }
            }
            token = strtok_r(NULL, ",;", &saveptr);
        }
    }
    return true;
}

bool db_update_concept(int id, const char *name, const char *description, const char *keywords) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_update_concept(id, name, description, keywords);
    }
    if (!g_db || id <= 0 || !name || !description) return false;

    sqlite3_stmt *stmt = NULL;
    const char *sql = "UPDATE concepts SET concept_name = ?, description = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, description, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return false;

    /* Update keyword associations */
    sqlite3_stmt *del = NULL;
    if (sqlite3_prepare_v2(g_db, "DELETE FROM concept_keywords WHERE concept_id = ?;", -1, &del, NULL) == SQLITE_OK) {
        sqlite3_bind_int(del, 1, id);
        sqlite3_step(del);
        sqlite3_finalize(del);
    }

    if (keywords && strlen(keywords) > 0) {
        char buf[512];
        strncpy(buf, keywords, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *saveptr = NULL;
        char *token = strtok_r(buf, ",;", &saveptr);
        while (token) {
            while (*token == ' ') token++;
            char *end = token + strlen(token) - 1;
            while (end > token && *end == ' ') { *end = '\0'; end--; }
            if (strlen(token) > 0) {
                sqlite3_stmt *ins = NULL;
                if (sqlite3_prepare_v2(g_db, "INSERT INTO concept_keywords (concept_id, keyword) VALUES (?, ?);", -1, &ins, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(ins, 1, id);
                    sqlite3_bind_text(ins, 2, token, -1, SQLITE_STATIC);
                    sqlite3_step(ins);
                    sqlite3_finalize(ins);
                }
            }
            token = strtok_r(NULL, ",;", &saveptr);
        }
    }
    return true;
}

bool db_delete_concept(int id) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_delete_concept(id);
    }
    if (!g_db || id <= 0) return false;

    sqlite3_stmt *del_kws = NULL;
    if (sqlite3_prepare_v2(g_db, "DELETE FROM concept_keywords WHERE concept_id = ?;", -1, &del_kws, NULL) == SQLITE_OK) {
        sqlite3_bind_int(del_kws, 1, id);
        sqlite3_step(del_kws);
        sqlite3_finalize(del_kws);
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, "DELETE FROM concepts WHERE id = ?;", -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

void db_free_concepts(DBConceptRecord *records) {
    if (records) free(records);
}

/* Load concepts into memory and hydrate inverted index */
size_t db_load_concepts_into_memory(ConceptIndex *idx) {
    if (g_driver_type == DB_DRIVER_SQLITE_CLOUD) {
        return db_cloud_load_concepts_into_memory(idx);
    }
    if (!g_db || !idx) return 0;

    /* Check if concepts table is empty, if so, seed defaults */
    sqlite3_stmt *chk = NULL;
    int concept_count = 0;
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM concepts;", -1, &chk, NULL) == SQLITE_OK) {
        if (sqlite3_step(chk) == SQLITE_ROW) concept_count = sqlite3_column_int(chk, 0);
        sqlite3_finalize(chk);
    }

    if (concept_count == 0) {
        /* Seed default concepts into SQLite database */
        db_add_concept("Memoization",
            "A technique that stores previously computed results to avoid repeated calculations and optimize recursion",
            "caching, dynamic programming, stored results, lookup table, cache", NULL);
        db_add_concept("Dijkstra Algorithm",
            "Algorithm that finds the shortest path between nodes in a weighted graph with non-negative edge weights",
            "shortest path, graph search, priority queue, single source shortest path", NULL);
        db_add_concept("HDMI Adapter",
            "Device or cable used to connect a laptop or computer to a projector, monitor or external display",
            "usb-c to hdmi converter, display adapter, projector connector, displayport to hdmi", NULL);
        db_add_concept("Laptop Cooling Pad",
            "External accessory used to cool a laptop and prevent thermal throttling or overheating with fans",
            "cooling stand, laptop cooler, fan pad, thermal mat", NULL);
        db_add_concept("Binary Search",
            "Search algorithm that finds the position of a target value within a sorted array by repeatedly dividing the search range in half",
            "divide and conquer, logarithmic search, sorted array lookup", NULL);
        db_add_concept("LRU Cache",
            "A caching data structure that evicts the least recently used elements first when capacity limit is reached",
            "least recently used, cache eviction policy, doubly linked list hashmap", NULL);
        db_add_concept("Depth First Search",
            "Graph traversal algorithm that explores as deep as possible along each branch before backtracking",
            "dfs, tree traversal, stack based search, recursion traversal", NULL);
        db_add_concept("Breadth First Search",
            "Graph traversal algorithm that explores all neighbor nodes at the present depth before moving to the next level",
            "bfs, level order traversal, queue search, unweighted shortest path", NULL);
        db_add_concept("Hash Table",
            "Data structure that implements an associative array abstract data type, mapping keys to values using a hash function",
            "hash map, dictionary, separate chaining, key value store", NULL);
        db_add_concept("Min Heap",
            "A complete binary tree data structure where the parent node is always less than or equal to its children",
            "priority queue, binary heap, top-k ranking, heap sort", NULL);
    }

    DBConceptRecord *records = NULL;
    size_t count = db_get_all_concepts(&records);
    if (count > 0 && records) {
        concept_index_clear(idx);
        for (size_t i = 0; i < count; i++) {
            concept_index_add(idx, records[i].id, records[i].concept_name, records[i].description, records[i].keywords);
        }
        db_free_concepts(records);
    }
    return count;
}

