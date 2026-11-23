#include "db_cloud.h"
#include "http_client.h"
#include "concept_index.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

static char g_gateway_url[512] = {0};
static char g_database[128] = "intellisuggest.sqlite";
static char g_api_key[256] = {0};
static bool g_cloud_initialized = false;

static void get_iso_now(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    if (tm) {
        strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", tm);
    } else {
        snprintf(buf, len, "1970-01-01T00:00:00Z");
    }
}

static void escape_sql(const char *in, char *out, size_t out_len) {
    if (!in || !out || out_len == 0) return;
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 2 < out_len; i++) {
        if (in[i] == '\'') {
            out[j++] = '\'';
            out[j++] = '\'';
        } else if (in[i] == '\\') {
            out[j++] = '\\';
            out[j++] = '\\';
        } else if ((unsigned char)in[i] >= 32 || in[i] == '\t') {
            out[j++] = in[i];
        }
    }
    out[j] = '\0';
}

/* Strict input validation for keywords to prevent injection and malformed tokens */
static bool validate_keyword_input(const char *in) {
    if (!in) return false;
    size_t len = strlen(in);
    if (len == 0 || len > 120) return false;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (!isalnum(c) && c != ' ' && c != '-' && c != '_' && c != '.') {
            return false;
        }
    }
    return true;
}

static bool parse_sqlitecloud_url(const char *url, char *gateway_url, size_t gw_len,
                                  char *database, size_t db_len,
                                  char *api_key, size_t key_len) {
    if (!url) return false;

    /* If already an https gateway URL */
    if (strncmp(url, "https://", 8) == 0) {
        strncpy(gateway_url, url, gw_len - 1);
        gateway_url[gw_len - 1] = '\0';
        return true;
    }

    const char *prefix = "sqlitecloud://";
    if (strncmp(url, prefix, strlen(prefix)) != 0) return false;

    const char *p = url + strlen(prefix);
    const char *slash = strchr(p, '/');
    if (!slash) return false;

    char host_port[256];
    size_t host_len = slash - p;
    if (host_len >= sizeof(host_port)) host_len = sizeof(host_port) - 1;
    strncpy(host_port, p, host_len);
    host_port[host_len] = '\0';

    char host[256];
    char *colon = strchr(host_port, ':');
    if (colon) *colon = '\0';
    strncpy(host, host_port, sizeof(host) - 1);
    host[sizeof(host) - 1] = '\0';

    char gateway_host[256];
    char *suffix = strstr(host, ".sqlite.cloud");
    if (suffix) {
        *suffix = '\0';
        snprintf(gateway_host, sizeof(gateway_host), "%s.gateway.sqlite.cloud", host);
    } else {
        strncpy(gateway_host, host, sizeof(gateway_host) - 1);
    }
    snprintf(gateway_url, gw_len, "https://%s/v2/weblite/sql", gateway_host);

    const char *db_start = slash + 1;
    const char *qmark = strchr(db_start, '?');
    if (qmark) {
        size_t dbl = qmark - db_start;
        if (dbl >= db_len) dbl = db_len - 1;
        strncpy(database, db_start, dbl);
        database[dbl] = '\0';

        const char *key_start = strstr(qmark + 1, "apikey=");
        if (key_start) {
            key_start += 7;
            const char *amp = strchr(key_start, '&');
            size_t kl = amp ? (size_t)(amp - key_start) : strlen(key_start);
            if (kl >= key_len) kl = key_len - 1;
            strncpy(api_key, key_start, kl);
            api_key[kl] = '\0';
        }
    } else {
        strncpy(database, db_start, db_len - 1);
    }
    return true;
}

/* Internal helper to execute a SQL string and return parsed JSON root */
static cJSON* execute_cloud_sql(const char *sql) {
    if (!g_cloud_initialized || !sql) return NULL;

    cJSON *req_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(req_obj, "sql", sql);
    cJSON_AddStringToObject(req_obj, "database", g_database);

    char *req_json = cJSON_PrintUnformatted(req_obj);
    cJSON_Delete(req_obj);
    if (!req_json) return NULL;

    HttpResponse resp;
    bool ok = http_post_json(g_gateway_url, g_api_key, req_json, &resp, 10000);
    free(req_json);

    if (!ok || !resp.body) {
        fprintf(stderr, "[SQLiteCloud Error] HTTP request failed: %s (status %d)\n",
                resp.error_msg, resp.status_code);
        http_response_free(&resp);
        return NULL;
    }

    cJSON *root = cJSON_Parse(resp.body);
    http_response_free(&resp);

    if (!root) {
        fprintf(stderr, "[SQLiteCloud Error] Failed to parse JSON response from gateway\n");
        return NULL;
    }

    cJSON *err = cJSON_GetObjectItem(root, "error");
    if (err) {
        cJSON *msg = cJSON_GetObjectItem(err, "message");
        fprintf(stderr, "[SQLiteCloud Error] Query error: %s\n",
                msg && msg->valuestring ? msg->valuestring : "Unknown error");
        cJSON_Delete(root);
        return NULL;
    }

    return root;
}

bool db_cloud_init(const char *sqlitecloud_url) {
    g_cloud_initialized = false;

    /* Check parameters or env vars */
    const char *url_env = sqlitecloud_url ? sqlitecloud_url : getenv("SQLITECLOUD_URL");
    const char *key_env = getenv("SQLITECLOUD_API_KEY");
    const char *db_env = getenv("SQLITECLOUD_DATABASE");
    const char *gw_env = getenv("SQLITECLOUD_GATEWAY_URL");

    if (url_env && strlen(url_env) > 0) {
        parse_sqlitecloud_url(url_env, g_gateway_url, sizeof(g_gateway_url),
                              g_database, sizeof(g_database),
                              g_api_key, sizeof(g_api_key));
    }

    if (gw_env && strlen(gw_env) > 0) {
        strncpy(g_gateway_url, gw_env, sizeof(g_gateway_url) - 1);
    }
    if (db_env && strlen(db_env) > 0) {
        strncpy(g_database, db_env, sizeof(g_database) - 1);
    }
    if (key_env && strlen(key_env) > 0) {
        strncpy(g_api_key, key_env, sizeof(g_api_key) - 1);
    }

    /* Fallback default database */
    if (strlen(g_database) == 0 || strcmp(g_database, "chinook.sqlite") == 0) {
        strncpy(g_database, "intellisuggest.sqlite", sizeof(g_database) - 1);
    }

    if (strlen(g_gateway_url) == 0 || strlen(g_api_key) == 0) {
        fprintf(stderr, "[SQLiteCloud Error] Missing gateway URL or API key in configuration.\n");
        return false;
    }

    g_cloud_initialized = true;

    /* Ping connection */
    cJSON *res = execute_cloud_sql("SELECT 1 AS ping;");
    if (!res) {
        fprintf(stderr, "[SQLiteCloud Error] Failed to connect to remote database '%s' at %s\n",
                g_database, g_gateway_url);
        g_cloud_initialized = false;
        return false;
    }
    cJSON_Delete(res);

    printf("[SQLiteCloud] Connected successfully to remote database '%s' via %s\n",
           g_database, g_gateway_url);

    /* Verify and create schema tables */
    if (!db_cloud_create_schema()) {
        fprintf(stderr, "[SQLiteCloud Error] Schema verification failed on cloud database\n");
        g_cloud_initialized = false;
        return false;
    }

    return true;
}

void db_cloud_close(void) {
    g_cloud_initialized = false;
    memset(g_api_key, 0, sizeof(g_api_key));
}

bool db_cloud_create_schema(void) {
    const char *stmts[] = {
        "CREATE TABLE IF NOT EXISTS keywords ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  keyword TEXT UNIQUE NOT NULL,"
        "  frequency INTEGER DEFAULT 0,"
        "  created_at TEXT NOT NULL,"
        "  last_searched_at TEXT,"
        "  is_active INTEGER DEFAULT 1"
        ");",
        "CREATE INDEX IF NOT EXISTS idx_keywords_keyword ON keywords(keyword);",
        "CREATE INDEX IF NOT EXISTS idx_keywords_frequency ON keywords(frequency DESC);",
        "CREATE INDEX IF NOT EXISTS idx_keywords_active ON keywords(is_active);",
        "CREATE TABLE IF NOT EXISTS search_history ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  keyword_id INTEGER NOT NULL,"
        "  searched_at TEXT NOT NULL,"
        "  FOREIGN KEY(keyword_id) REFERENCES keywords(id) ON DELETE CASCADE"
        ");",
        "CREATE INDEX IF NOT EXISTS idx_search_history_keyword ON search_history(keyword_id);",
        "CREATE INDEX IF NOT EXISTS idx_search_history_time ON search_history(searched_at DESC);",
        "CREATE TABLE IF NOT EXISTS concepts ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  concept_name TEXT UNIQUE NOT NULL,"
        "  description TEXT NOT NULL"
        ");",
        "CREATE TABLE IF NOT EXISTS concept_keywords ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  concept_id INTEGER NOT NULL,"
        "  keyword TEXT NOT NULL,"
        "  FOREIGN KEY(concept_id) REFERENCES concepts(id) ON DELETE CASCADE"
        ");",
        "CREATE INDEX IF NOT EXISTS idx_concept_keywords_concept ON concept_keywords(concept_id);",
        "CREATE TABLE IF NOT EXISTS search_metrics ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  metric_name TEXT NOT NULL,"
        "  metric_value REAL NOT NULL,"
        "  recorded_at TEXT NOT NULL"
        ");"
    };

    for (size_t i = 0; i < sizeof(stmts)/sizeof(stmts[0]); i++) {
        cJSON *res = execute_cloud_sql(stmts[i]);
        if (!res) return false;
        cJSON_Delete(res);
    }
    return true;
}

bool db_cloud_seed_from_csv(const char *csv_path) {
    if (!g_cloud_initialized || !csv_path) return false;

    /* Check if already populated */
    cJSON *check = execute_cloud_sql("SELECT count(*) AS cnt FROM keywords WHERE is_active = 1;");
    if (check) {
        cJSON *data = cJSON_GetObjectItem(check, "data");
        if (cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0) {
            cJSON *row0 = cJSON_GetArrayItem(data, 0);
            cJSON *cnt = cJSON_GetObjectItem(row0, "cnt");
            if (cnt && cnt->valueint > 5) {
                cJSON_Delete(check);
                return true; /* Already seeded */
            }
        }
        cJSON_Delete(check);
    }

    FILE *f = fopen(csv_path, "r");
    if (!f) return false;

    char line[256];
    char now[32];
    get_iso_now(now, sizeof(now));

    if (fgets(line, sizeof(line), f)) {
        if (strstr(line, "keyword") == NULL) rewind(f);
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
            char esc_kw[256];
            escape_sql(norm, esc_kw, sizeof(esc_kw));

            char sql[512];
            snprintf(sql, sizeof(sql),
                     "INSERT INTO keywords (keyword, frequency, created_at, is_active) "
                     "VALUES ('%s', %d, '%s', 1) "
                     "ON CONFLICT(keyword) DO UPDATE SET frequency = %d;",
                     esc_kw, freq, now, freq);
            cJSON *res = execute_cloud_sql(sql);
            if (res) cJSON_Delete(res);
            free(norm);
        }
    }
    fclose(f);
    return true;
}

size_t db_cloud_load_keywords_into_memory(Trie *trie, HashTable *ht) {
    if (!g_cloud_initialized || !trie || !ht) return 0;

    cJSON *res = execute_cloud_sql("SELECT keyword, frequency, created_at, COALESCE(last_searched_at, '') AS last_searched_at, is_active FROM keywords WHERE is_active = 1;");
    if (!res) return 0;

    cJSON *data = cJSON_GetObjectItem(res, "data");
    if (!cJSON_IsArray(data)) {
        cJSON_Delete(res);
        return 0;
    }

    size_t count = 0;
    cJSON *row = NULL;
    cJSON_ArrayForEach(row, data) {
        cJSON *kw_item = cJSON_GetObjectItem(row, "keyword");
        cJSON *fr_item = cJSON_GetObjectItem(row, "frequency");
        cJSON *cr_item = cJSON_GetObjectItem(row, "created_at");
        cJSON *ls_item = cJSON_GetObjectItem(row, "last_searched_at");
        cJSON *ia_item = cJSON_GetObjectItem(row, "is_active");

        if (kw_item && kw_item->valuestring) {
            int freq = fr_item ? fr_item->valueint : 1;
            char *norm = trie_normalize(kw_item->valuestring);
            if (norm && strlen(norm) > 0) {
                trie_insert(trie, norm, freq);
                hashtable_insert(ht, norm, freq,
                                 cr_item ? cr_item->valuestring : NULL,
                                 ls_item ? ls_item->valuestring : NULL,
                                 ia_item ? (ia_item->valueint != 0) : true);
                count++;
                free(norm);
            }
        }
    }

    cJSON_Delete(res);
    return count;
}

bool db_cloud_record_search(const char *keyword, int *out_new_frequency, const char *searched_at) {
    if (!g_cloud_initialized || !keyword) return false;
    if (!validate_keyword_input(keyword)) return false;

    char *norm = trie_normalize(keyword);
    if (!norm) return false;

    char now[32];
    if (searched_at && strlen(searched_at) > 0) {
        strncpy(now, searched_at, sizeof(now) - 1);
        now[sizeof(now) - 1] = '\0';
    } else {
        get_iso_now(now, sizeof(now));
    }

    char esc[256];
    escape_sql(norm, esc, sizeof(esc));
    free(norm);

    /* Atomic Upsert keyword frequency */
    char upsert_sql[512];
    snprintf(upsert_sql, sizeof(upsert_sql),
             "INSERT INTO keywords(keyword, frequency, created_at, last_searched_at, is_active) "
             "VALUES('%s', 1, '%s', '%s', 1) "
             "ON CONFLICT(keyword) DO UPDATE SET frequency = frequency + 1, last_searched_at = '%s', is_active = 1;",
             esc, now, now, now);

    cJSON *r1 = execute_cloud_sql(upsert_sql);
    if (!r1) return false;
    cJSON_Delete(r1);

    /* Record search history */
    char hist_sql[512];
    snprintf(hist_sql, sizeof(hist_sql),
             "INSERT INTO search_history(keyword_id, searched_at) "
             "SELECT id, '%s' FROM keywords WHERE keyword = '%s';",
             now, esc);
    cJSON *r2 = execute_cloud_sql(hist_sql);
    if (r2) cJSON_Delete(r2);

    /* Query updated frequency */
    char sel_sql[256];
    snprintf(sel_sql, sizeof(sel_sql), "SELECT frequency FROM keywords WHERE keyword = '%s';", esc);
    cJSON *r3 = execute_cloud_sql(sel_sql);
    if (r3) {
        cJSON *data = cJSON_GetObjectItem(r3, "data");
        if (cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0) {
            cJSON *row0 = cJSON_GetArrayItem(data, 0);
            cJSON *fr = cJSON_GetObjectItem(row0, "frequency");
            if (fr && out_new_frequency) {
                *out_new_frequency = fr->valueint;
            }
        }
        cJSON_Delete(r3);
    }

    return true;
}

bool db_cloud_add_keyword(const char *keyword, int initial_frequency, int *out_id) {
    if (!g_cloud_initialized || !keyword) return false;

    char *norm = trie_normalize(keyword);
    if (!norm) return false;

    char now[32];
    get_iso_now(now, sizeof(now));

    char esc[256];
    escape_sql(norm, esc, sizeof(esc));
    free(norm);

    char sql[512];
    snprintf(sql, sizeof(sql),
             "INSERT INTO keywords (keyword, frequency, created_at, is_active) "
             "VALUES ('%s', %d, '%s', 1) "
             "ON CONFLICT(keyword) DO UPDATE SET frequency = %d, is_active = 1;",
             esc, initial_frequency, now, initial_frequency);

    cJSON *res = execute_cloud_sql(sql);
    if (!res) return false;
    cJSON_Delete(res);

    if (out_id) {
        char sel_sql[256];
        snprintf(sel_sql, sizeof(sel_sql), "SELECT id FROM keywords WHERE keyword = '%s';", esc);
        cJSON *r2 = execute_cloud_sql(sel_sql);
        if (r2) {
            cJSON *data = cJSON_GetObjectItem(r2, "data");
            if (cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0) {
                cJSON *row0 = cJSON_GetArrayItem(data, 0);
                cJSON *id_item = cJSON_GetObjectItem(row0, "id");
                if (id_item) *out_id = id_item->valueint;
            }
            cJSON_Delete(r2);
        }
    }
    return true;
}

bool db_cloud_update_keyword(int id, const char *keyword, int frequency, bool is_active) {
    if (!g_cloud_initialized || !keyword) return false;

    char *norm = trie_normalize(keyword);
    if (!norm) return false;

    char esc[256];
    escape_sql(norm, esc, sizeof(esc));
    free(norm);

    char sql[512];
    snprintf(sql, sizeof(sql),
             "UPDATE keywords SET keyword = '%s', frequency = %d, is_active = %d WHERE id = %d;",
             esc, frequency, is_active ? 1 : 0, id);

    cJSON *res = execute_cloud_sql(sql);
    if (!res) return false;
    cJSON_Delete(res);
    return true;
}

bool db_cloud_delete_keyword(int id, char *out_deleted_keyword, size_t max_len) {
    if (!g_cloud_initialized) return false;

    char sel_sql[256];
    snprintf(sel_sql, sizeof(sel_sql), "SELECT keyword FROM keywords WHERE id = %d;", id);
    cJSON *r1 = execute_cloud_sql(sel_sql);
    if (!r1) return false;

    cJSON *data = cJSON_GetObjectItem(r1, "data");
    if (!cJSON_IsArray(data) || cJSON_GetArraySize(data) == 0) {
        cJSON_Delete(r1);
        return false;
    }

    cJSON *row0 = cJSON_GetArrayItem(data, 0);
    cJSON *kw_item = cJSON_GetObjectItem(row0, "keyword");
    if (kw_item && kw_item->valuestring && out_deleted_keyword) {
        strncpy(out_deleted_keyword, kw_item->valuestring, max_len - 1);
        out_deleted_keyword[max_len - 1] = '\0';
    }
    cJSON_Delete(r1);

    char del_sql[256];
    snprintf(del_sql, sizeof(del_sql), "DELETE FROM keywords WHERE id = %d;", id);
    cJSON *r2 = execute_cloud_sql(del_sql);
    if (!r2) return false;
    cJSON_Delete(r2);

    return true;
}

size_t db_cloud_cleanup_expired(int days_threshold, char ***out_deleted_keywords) {
    if (!g_cloud_initialized) return 0;

    time_t cutoff = time(NULL) - ((time_t)days_threshold * 86400);
    struct tm *tm = gmtime(&cutoff);
    char cutoff_str[32];
    if (tm) strftime(cutoff_str, sizeof(cutoff_str), "%Y-%m-%dT%H:%M:%SZ", tm);
    else snprintf(cutoff_str, sizeof(cutoff_str), "1970-01-01T00:00:00Z");

    char sel_sql[512];
    snprintf(sel_sql, sizeof(sel_sql),
             "SELECT keyword FROM keywords WHERE is_active = 1 AND ("
             "(last_searched_at IS NOT NULL AND last_searched_at < '%s') OR "
             "(last_searched_at IS NULL AND created_at < '%s'));",
             cutoff_str, cutoff_str);

    cJSON *res = execute_cloud_sql(sel_sql);
    if (!res) return 0;

    cJSON *data = cJSON_GetObjectItem(res, "data");
    if (!cJSON_IsArray(data)) {
        cJSON_Delete(res);
        return 0;
    }

    size_t count = (size_t)cJSON_GetArraySize(data);
    if (count == 0) {
        cJSON_Delete(res);
        return 0;
    }

    char **list = (char**)calloc(count, sizeof(char*));
    for (size_t i = 0; i < count; i++) {
        cJSON *row = cJSON_GetArrayItem(data, (int)i);
        cJSON *kw = cJSON_GetObjectItem(row, "keyword");
        if (kw && kw->valuestring) {
            list[i] = strdup(kw->valuestring);
        } else {
            list[i] = strdup("");
        }
    }
    cJSON_Delete(res);

    char upd_sql[512];
    snprintf(upd_sql, sizeof(upd_sql),
             "UPDATE keywords SET is_active = 0 WHERE is_active = 1 AND ("
             "(last_searched_at IS NOT NULL AND last_searched_at < '%s') OR "
             "(last_searched_at IS NULL AND created_at < '%s'));",
             cutoff_str, cutoff_str);
    cJSON *r2 = execute_cloud_sql(upd_sql);
    if (r2) cJSON_Delete(r2);

    if (out_deleted_keywords) {
        *out_deleted_keywords = list;
    } else {
        for (size_t i = 0; i < count; i++) free(list[i]);
        free(list);
    }
    return count;
}

size_t db_cloud_get_recent_history(size_t limit, DBHistoryRecord **out_records) {
    if (!g_cloud_initialized || !out_records) return 0;

    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT sh.id, k.keyword, sh.searched_at "
             "FROM search_history sh JOIN keywords k ON sh.keyword_id = k.id "
             "ORDER BY sh.id DESC LIMIT %zu;", limit > 0 ? limit : 20);

    cJSON *res = execute_cloud_sql(sql);
    if (!res) return 0;

    cJSON *data = cJSON_GetObjectItem(res, "data");
    if (!cJSON_IsArray(data)) {
        cJSON_Delete(res);
        return 0;
    }

    size_t count = (size_t)cJSON_GetArraySize(data);
    if (count == 0) {
        cJSON_Delete(res);
        *out_records = NULL;
        return 0;
    }

    DBHistoryRecord *records = (DBHistoryRecord*)calloc(count, sizeof(DBHistoryRecord));
    for (size_t i = 0; i < count; i++) {
        cJSON *row = cJSON_GetArrayItem(data, (int)i);
        cJSON *id = cJSON_GetObjectItem(row, "id");
        cJSON *kw = cJSON_GetObjectItem(row, "keyword");
        cJSON *sa = cJSON_GetObjectItem(row, "searched_at");

        records[i].id = id ? id->valueint : 0;
        if (kw && kw->valuestring) strncpy(records[i].keyword, kw->valuestring, sizeof(records[i].keyword) - 1);
        if (sa && sa->valuestring) strncpy(records[i].searched_at, sa->valuestring, sizeof(records[i].searched_at) - 1);
    }

    cJSON_Delete(res);
    *out_records = records;
    return count;
}

size_t db_cloud_get_popular_keywords(size_t limit, DBKeywordRecord **out_records) {
    if (!g_cloud_initialized || !out_records) return 0;

    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT id, keyword, frequency, created_at, COALESCE(last_searched_at, '') AS last_searched_at, is_active "
             "FROM keywords WHERE is_active = 1 ORDER BY frequency DESC LIMIT %zu;", limit > 0 ? limit : 10);

    cJSON *res = execute_cloud_sql(sql);
    if (!res) return 0;

    cJSON *data = cJSON_GetObjectItem(res, "data");
    if (!cJSON_IsArray(data)) {
        cJSON_Delete(res);
        return 0;
    }

    size_t count = (size_t)cJSON_GetArraySize(data);
    if (count == 0) {
        cJSON_Delete(res);
        *out_records = NULL;
        return 0;
    }

    DBKeywordRecord *records = (DBKeywordRecord*)calloc(count, sizeof(DBKeywordRecord));
    for (size_t i = 0; i < count; i++) {
        cJSON *row = cJSON_GetArrayItem(data, (int)i);
        cJSON *id = cJSON_GetObjectItem(row, "id");
        cJSON *kw = cJSON_GetObjectItem(row, "keyword");
        cJSON *fr = cJSON_GetObjectItem(row, "frequency");
        cJSON *ca = cJSON_GetObjectItem(row, "created_at");
        cJSON *la = cJSON_GetObjectItem(row, "last_searched_at");
        cJSON *ia = cJSON_GetObjectItem(row, "is_active");

        records[i].id = id ? id->valueint : 0;
        records[i].frequency = fr ? fr->valueint : 0;
        records[i].is_active = ia ? (ia->valueint != 0) : true;
        if (kw && kw->valuestring) strncpy(records[i].keyword, kw->valuestring, sizeof(records[i].keyword) - 1);
        if (ca && ca->valuestring) strncpy(records[i].created_at, ca->valuestring, sizeof(records[i].created_at) - 1);
        if (la && la->valuestring) strncpy(records[i].last_searched_at, la->valuestring, sizeof(records[i].last_searched_at) - 1);
    }

    cJSON_Delete(res);
    *out_records = records;
    return count;
}

size_t db_cloud_get_all_keywords(DBKeywordRecord **out_records) {
    if (!g_cloud_initialized || !out_records) return 0;

    const char *sql = "SELECT id, keyword, frequency, created_at, COALESCE(last_searched_at, '') AS last_searched_at, is_active "
                      "FROM keywords ORDER BY frequency DESC;";

    cJSON *res = execute_cloud_sql(sql);
    if (!res) return 0;

    cJSON *data = cJSON_GetObjectItem(res, "data");
    if (!cJSON_IsArray(data)) {
        cJSON_Delete(res);
        return 0;
    }

    size_t count = (size_t)cJSON_GetArraySize(data);
    if (count == 0) {
        cJSON_Delete(res);
        *out_records = NULL;
        return 0;
    }

    DBKeywordRecord *records = (DBKeywordRecord*)calloc(count, sizeof(DBKeywordRecord));
    for (size_t i = 0; i < count; i++) {
        cJSON *row = cJSON_GetArrayItem(data, (int)i);
        cJSON *id = cJSON_GetObjectItem(row, "id");
        cJSON *kw = cJSON_GetObjectItem(row, "keyword");
        cJSON *fr = cJSON_GetObjectItem(row, "frequency");
        cJSON *ca = cJSON_GetObjectItem(row, "created_at");
        cJSON *la = cJSON_GetObjectItem(row, "last_searched_at");
        cJSON *ia = cJSON_GetObjectItem(row, "is_active");

        records[i].id = id ? id->valueint : 0;
        records[i].frequency = fr ? fr->valueint : 0;
        records[i].is_active = ia ? (ia->valueint != 0) : true;
        if (kw && kw->valuestring) strncpy(records[i].keyword, kw->valuestring, sizeof(records[i].keyword) - 1);
        if (ca && ca->valuestring) strncpy(records[i].created_at, ca->valuestring, sizeof(records[i].created_at) - 1);
        if (la && la->valuestring) strncpy(records[i].last_searched_at, la->valuestring, sizeof(records[i].last_searched_at) - 1);
    }

    cJSON_Delete(res);
    *out_records = records;
    return count;
}

bool db_cloud_get_analytics_summary(DBAnalyticsSummary *out_summary) {
    if (!g_cloud_initialized || !out_summary) return false;
    memset(out_summary, 0, sizeof(*out_summary));

    const char *sql =
        "SELECT "
        "(SELECT count(*) FROM keywords) AS total_kw, "
        "(SELECT count(*) FROM keywords WHERE is_active = 1) AS active_kw, "
        "(SELECT count(*) FROM search_history) AS total_sh, "
        "(SELECT count(*) FROM keywords WHERE is_active = 0) AS expired_kw;";

    cJSON *res = execute_cloud_sql(sql);
    if (!res) return false;

    cJSON *data = cJSON_GetObjectItem(res, "data");
    if (cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0) {
        cJSON *row0 = cJSON_GetArrayItem(data, 0);
        cJSON *tkw = cJSON_GetObjectItem(row0, "total_kw");
        cJSON *akw = cJSON_GetObjectItem(row0, "active_kw");
        cJSON *tsh = cJSON_GetObjectItem(row0, "total_sh");
        cJSON *ekw = cJSON_GetObjectItem(row0, "expired_kw");

        if (tkw) out_summary->total_keywords = (size_t)tkw->valueint;
        if (akw) out_summary->active_keywords = (size_t)akw->valueint;
        if (tsh) out_summary->total_searches = (size_t)tsh->valueint;
        if (ekw) out_summary->expired_keywords = (size_t)ekw->valueint;
    }

    cJSON_Delete(res);
    return true;
}

/* Trending Keywords from SQLiteCloud */
size_t db_cloud_get_trending_keywords(size_t limit, int hours_window, DBKeywordRecord **out_records) {
    if (!g_cloud_initialized || !out_records) return 0;
    if (hours_window <= 0) hours_window = 24;
    if (limit == 0) limit = 10;

    time_t cutoff_t = time(NULL) - (time_t)(hours_window * 3600);
    char cutoff_str[32];
    struct tm *tm = gmtime(&cutoff_t);
    if (tm) strftime(cutoff_str, sizeof(cutoff_str), "%Y-%m-%dT%H:%M:%SZ", tm);
    else snprintf(cutoff_str, sizeof(cutoff_str), "1970-01-01T00:00:00Z");

    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT id, keyword, frequency, created_at, COALESCE(last_searched_at, '') AS last_searched_at, is_active "
             "FROM keywords WHERE is_active = 1 AND last_searched_at >= '%s' "
             "ORDER BY frequency DESC LIMIT %zu;", cutoff_str, limit);

    cJSON *res = execute_cloud_sql(sql);
    size_t count = 0;
    DBKeywordRecord *records = (DBKeywordRecord*)calloc(limit, sizeof(DBKeywordRecord));

    if (res) {
        cJSON *data = cJSON_GetObjectItem(res, "data");
        if (cJSON_IsArray(data)) {
            cJSON *row = NULL;
            cJSON_ArrayForEach(row, data) {
                if (count >= limit) break;
                cJSON *id = cJSON_GetObjectItem(row, "id");
                cJSON *kw = cJSON_GetObjectItem(row, "keyword");
                cJSON *fr = cJSON_GetObjectItem(row, "frequency");
                cJSON *ca = cJSON_GetObjectItem(row, "created_at");
                cJSON *la = cJSON_GetObjectItem(row, "last_searched_at");
                cJSON *ia = cJSON_GetObjectItem(row, "is_active");

                records[count].id = id ? id->valueint : 0;
                records[count].frequency = fr ? fr->valueint : 0;
                records[count].is_active = ia ? (ia->valueint != 0) : true;
                if (kw && kw->valuestring) strncpy(records[count].keyword, kw->valuestring, sizeof(records[count].keyword) - 1);
                if (ca && ca->valuestring) strncpy(records[count].created_at, ca->valuestring, sizeof(records[count].created_at) - 1);
                if (la && la->valuestring) strncpy(records[count].last_searched_at, la->valuestring, sizeof(records[count].last_searched_at) - 1);
                count++;
            }
        }
        cJSON_Delete(res);
    }

    /* Fallback if few recent searches */
    if (count < limit) {
        DBKeywordRecord *pop = NULL;
        size_t pop_count = db_cloud_get_popular_keywords(limit, &pop);
        for (size_t i = 0; i < pop_count && count < limit; i++) {
            bool already = false;
            for (size_t j = 0; j < count; j++) {
                if (records[j].id == pop[i].id) { already = true; break; }
            }
            if (!already) {
                records[count++] = pop[i];
            }
        }
        if (pop) free(pop);
    }

    *out_records = records;
    return count;
}

/* Expired Keywords from SQLiteCloud */
size_t db_cloud_get_expired_keywords(int days_threshold, DBKeywordRecord **out_records) {
    if (!g_cloud_initialized || !out_records) return 0;
    if (days_threshold <= 0) days_threshold = 90;

    time_t cutoff_t = time(NULL) - (time_t)(days_threshold * 86400);
    char cutoff_str[32];
    struct tm *tm = gmtime(&cutoff_t);
    if (tm) strftime(cutoff_str, sizeof(cutoff_str), "%Y-%m-%dT%H:%M:%SZ", tm);
    else snprintf(cutoff_str, sizeof(cutoff_str), "1970-01-01T00:00:00Z");

    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT id, keyword, frequency, created_at, COALESCE(last_searched_at, '') AS last_searched_at, is_active "
             "FROM keywords WHERE is_active = 0 OR ("
             "(last_searched_at IS NOT NULL AND last_searched_at < '%s') OR "
             "(last_searched_at IS NULL AND created_at < '%s')) "
             "ORDER BY id DESC LIMIT 50;", cutoff_str, cutoff_str);

    cJSON *res = execute_cloud_sql(sql);
    if (!res) { *out_records = NULL; return 0; }

    cJSON *data = cJSON_GetObjectItem(res, "data");
    if (!cJSON_IsArray(data)) { cJSON_Delete(res); *out_records = NULL; return 0; }

    size_t count = (size_t)cJSON_GetArraySize(data);
    if (count == 0) { cJSON_Delete(res); *out_records = NULL; return 0; }

    DBKeywordRecord *records = (DBKeywordRecord*)calloc(count, sizeof(DBKeywordRecord));
    for (size_t i = 0; i < count; i++) {
        cJSON *row = cJSON_GetArrayItem(data, (int)i);
        cJSON *id = cJSON_GetObjectItem(row, "id");
        cJSON *kw = cJSON_GetObjectItem(row, "keyword");
        cJSON *fr = cJSON_GetObjectItem(row, "frequency");
        cJSON *ca = cJSON_GetObjectItem(row, "created_at");
        cJSON *la = cJSON_GetObjectItem(row, "last_searched_at");
        cJSON *ia = cJSON_GetObjectItem(row, "is_active");

        records[i].id = id ? id->valueint : 0;
        records[i].frequency = fr ? fr->valueint : 0;
        records[i].is_active = ia ? (ia->valueint != 0) : false;
        if (kw && kw->valuestring) strncpy(records[i].keyword, kw->valuestring, sizeof(records[i].keyword) - 1);
        if (ca && ca->valuestring) strncpy(records[i].created_at, ca->valuestring, sizeof(records[i].created_at) - 1);
        if (la && la->valuestring) strncpy(records[i].last_searched_at, la->valuestring, sizeof(records[i].last_searched_at) - 1);
    }

    cJSON_Delete(res);
    *out_records = records;
    return count;
}

/* Concept Management in SQLiteCloud */
size_t db_cloud_get_all_concepts(DBConceptRecord **out_records) {
    if (!g_cloud_initialized || !out_records) return 0;

    const char *sql =
        "SELECT c.id, c.concept_name, c.description, COALESCE(GROUP_CONCAT(ck.keyword, ', '), '') AS keywords "
        "FROM concepts c LEFT JOIN concept_keywords ck ON c.id = ck.concept_id "
        "GROUP BY c.id ORDER BY c.id ASC;";

    cJSON *res = execute_cloud_sql(sql);
    if (!res) { *out_records = NULL; return 0; }

    cJSON *data = cJSON_GetObjectItem(res, "data");
    if (!cJSON_IsArray(data)) { cJSON_Delete(res); *out_records = NULL; return 0; }

    size_t count = (size_t)cJSON_GetArraySize(data);
    if (count == 0) { cJSON_Delete(res); *out_records = NULL; return 0; }

    DBConceptRecord *records = (DBConceptRecord*)calloc(count, sizeof(DBConceptRecord));
    for (size_t i = 0; i < count; i++) {
        cJSON *row = cJSON_GetArrayItem(data, (int)i);
        cJSON *id = cJSON_GetObjectItem(row, "id");
        cJSON *nm = cJSON_GetObjectItem(row, "concept_name");
        cJSON *ds = cJSON_GetObjectItem(row, "description");
        cJSON *kw = cJSON_GetObjectItem(row, "keywords");

        records[i].id = id ? id->valueint : 0;
        if (nm && nm->valuestring) strncpy(records[i].concept_name, nm->valuestring, sizeof(records[i].concept_name) - 1);
        if (ds && ds->valuestring) strncpy(records[i].description, ds->valuestring, sizeof(records[i].description) - 1);
        if (kw && kw->valuestring) strncpy(records[i].keywords, kw->valuestring, sizeof(records[i].keywords) - 1);
    }

    cJSON_Delete(res);
    *out_records = records;
    return count;
}

bool db_cloud_get_concept_by_id(int id, DBConceptRecord *out_record) {
    if (!g_cloud_initialized || !out_record || id <= 0) return false;

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT c.id, c.concept_name, c.description, COALESCE(GROUP_CONCAT(ck.keyword, ', '), '') AS keywords "
             "FROM concepts c LEFT JOIN concept_keywords ck ON c.id = ck.concept_id "
             "WHERE c.id = %d GROUP BY c.id;", id);

    cJSON *res = execute_cloud_sql(sql);
    if (!res) return false;

    cJSON *data = cJSON_GetObjectItem(res, "data");
    bool found = false;
    if (cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0) {
        cJSON *row = cJSON_GetArrayItem(data, 0);
        cJSON *cid = cJSON_GetObjectItem(row, "id");
        cJSON *nm = cJSON_GetObjectItem(row, "concept_name");
        cJSON *ds = cJSON_GetObjectItem(row, "description");
        cJSON *kw = cJSON_GetObjectItem(row, "keywords");

        out_record->id = cid ? cid->valueint : id;
        if (nm && nm->valuestring) strncpy(out_record->concept_name, nm->valuestring, sizeof(out_record->concept_name) - 1);
        if (ds && ds->valuestring) strncpy(out_record->description, ds->valuestring, sizeof(out_record->description) - 1);
        if (kw && kw->valuestring) strncpy(out_record->keywords, kw->valuestring, sizeof(out_record->keywords) - 1);
        found = true;
    }
    cJSON_Delete(res);
    return found;
}

bool db_cloud_add_concept(const char *name, const char *description, const char *keywords, int *out_id) {
    if (!g_cloud_initialized || !name || !description) return false;

    char esc_nm[256], esc_ds[1024];
    escape_sql(name, esc_nm, sizeof(esc_nm));
    escape_sql(description, esc_ds, sizeof(esc_ds));

    char sql[1536];
    snprintf(sql, sizeof(sql),
             "INSERT INTO concepts (concept_name, description) VALUES ('%s', '%s') "
             "ON CONFLICT(concept_name) DO UPDATE SET description = '%s';",
             esc_nm, esc_ds, esc_ds);

    cJSON *res = execute_cloud_sql(sql);
    if (!res) return false;
    cJSON_Delete(res);

    int concept_id = 0;
    char sel_sql[512];
    snprintf(sel_sql, sizeof(sel_sql), "SELECT id FROM concepts WHERE concept_name = '%s';", esc_nm);
    cJSON *r2 = execute_cloud_sql(sel_sql);
    if (r2) {
        cJSON *data = cJSON_GetObjectItem(r2, "data");
        if (cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0) {
            cJSON *row = cJSON_GetArrayItem(data, 0);
            cJSON *id_item = cJSON_GetObjectItem(row, "id");
            if (id_item) concept_id = id_item->valueint;
        }
        cJSON_Delete(r2);
    }
    if (out_id) *out_id = concept_id;

    if (concept_id > 0 && keywords && strlen(keywords) > 0) {
        char del_sql[256];
        snprintf(del_sql, sizeof(del_sql), "DELETE FROM concept_keywords WHERE concept_id = %d;", concept_id);
        cJSON *rd = execute_cloud_sql(del_sql);
        if (rd) cJSON_Delete(rd);

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
                char esc_kw[256];
                escape_sql(token, esc_kw, sizeof(esc_kw));
                char ins_sql[512];
                snprintf(ins_sql, sizeof(ins_sql),
                         "INSERT INTO concept_keywords (concept_id, keyword) VALUES (%d, '%s');",
                         concept_id, esc_kw);
                cJSON *ri = execute_cloud_sql(ins_sql);
                if (ri) cJSON_Delete(ri);
            }
            token = strtok_r(NULL, ",;", &saveptr);
        }
    }
    return true;
}

bool db_cloud_update_concept(int id, const char *name, const char *description, const char *keywords) {
    if (!g_cloud_initialized || id <= 0 || !name || !description) return false;

    char esc_nm[256], esc_ds[1024];
    escape_sql(name, esc_nm, sizeof(esc_nm));
    escape_sql(description, esc_ds, sizeof(esc_ds));

    char sql[1536];
    snprintf(sql, sizeof(sql),
             "UPDATE concepts SET concept_name = '%s', description = '%s' WHERE id = %d;",
             esc_nm, esc_ds, id);

    cJSON *res = execute_cloud_sql(sql);
    if (!res) return false;
    cJSON_Delete(res);

    char del_sql[256];
    snprintf(del_sql, sizeof(del_sql), "DELETE FROM concept_keywords WHERE concept_id = %d;", id);
    cJSON *rd = execute_cloud_sql(del_sql);
    if (rd) cJSON_Delete(rd);

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
                char esc_kw[256];
                escape_sql(token, esc_kw, sizeof(esc_kw));
                char ins_sql[512];
                snprintf(ins_sql, sizeof(ins_sql),
                         "INSERT INTO concept_keywords (concept_id, keyword) VALUES (%d, '%s');",
                         id, esc_kw);
                cJSON *ri = execute_cloud_sql(ins_sql);
                if (ri) cJSON_Delete(ri);
            }
            token = strtok_r(NULL, ",;", &saveptr);
        }
    }
    return true;
}

bool db_cloud_delete_concept(int id) {
    if (!g_cloud_initialized || id <= 0) return false;

    char del_kws[256];
    snprintf(del_kws, sizeof(del_kws), "DELETE FROM concept_keywords WHERE concept_id = %d;", id);
    cJSON *r1 = execute_cloud_sql(del_kws);
    if (r1) cJSON_Delete(r1);

    char del_c[256];
    snprintf(del_c, sizeof(del_c), "DELETE FROM concepts WHERE id = %d;", id);
    cJSON *r2 = execute_cloud_sql(del_c);
    if (!r2) return false;
    cJSON_Delete(r2);

    return true;
}

size_t db_cloud_load_concepts_into_memory(ConceptIndex *idx) {
    if (!g_cloud_initialized || !idx) return 0;

    /* Check if cloud concepts empty, if so, seed defaults */
    cJSON *chk = execute_cloud_sql("SELECT count(*) AS cnt FROM concepts;");
    int count_in_db = 0;
    if (chk) {
        cJSON *data = cJSON_GetObjectItem(chk, "data");
        if (cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0) {
            cJSON *row = cJSON_GetArrayItem(data, 0);
            cJSON *cnt = cJSON_GetObjectItem(row, "cnt");
            if (cnt) count_in_db = cnt->valueint;
        }
        cJSON_Delete(chk);
    }

    if (count_in_db == 0) {
        db_cloud_add_concept("Memoization",
            "A technique that stores previously computed results to avoid repeated calculations and optimize recursion",
            "caching, dynamic programming, stored results, lookup table, cache", NULL);
        db_cloud_add_concept("Dijkstra Algorithm",
            "Algorithm that finds the shortest path between nodes in a weighted graph with non-negative edge weights",
            "shortest path, graph search, priority queue, single source shortest path", NULL);
        db_cloud_add_concept("HDMI Adapter",
            "Device or cable used to connect a laptop or computer to a projector, monitor or external display",
            "usb-c to hdmi converter, display adapter, projector connector, displayport to hdmi", NULL);
        db_cloud_add_concept("Laptop Cooling Pad",
            "External accessory used to cool a laptop and prevent thermal throttling or overheating with fans",
            "cooling stand, laptop cooler, fan pad, thermal mat", NULL);
        db_cloud_add_concept("Binary Search",
            "Search algorithm that finds the position of a target value within a sorted array by repeatedly dividing the search range in half",
            "divide and conquer, logarithmic search, sorted array lookup", NULL);
        db_cloud_add_concept("LRU Cache",
            "A caching data structure that evicts the least recently used elements first when capacity limit is reached",
            "least recently used, cache eviction policy, doubly linked list hashmap", NULL);
        db_cloud_add_concept("Depth First Search",
            "Graph traversal algorithm that explores as deep as possible along each branch before backtracking",
            "dfs, tree traversal, stack based search, recursion traversal", NULL);
        db_cloud_add_concept("Breadth First Search",
            "Graph traversal algorithm that explores all neighbor nodes at the present depth before moving to the next level",
            "bfs, level order traversal, queue search, unweighted shortest path", NULL);
        db_cloud_add_concept("Hash Table",
            "Data structure that implements an associative array abstract data type, mapping keys to values using a hash function",
            "hash map, dictionary, separate chaining, key value store", NULL);
        db_cloud_add_concept("Min Heap",
            "A complete binary tree data structure where the parent node is always less than or equal to its children",
            "priority queue, binary heap, top-k ranking, heap sort", NULL);
    }

    DBConceptRecord *records = NULL;
    size_t count = db_cloud_get_all_concepts(&records);
    if (count > 0 && records) {
        concept_index_clear(idx);
        for (size_t i = 0; i < count; i++) {
            concept_index_add(idx, records[i].id, records[i].concept_name, records[i].description, records[i].keywords);
        }
        free(records);
    }
    return count;
}
