#include "api.h"
#include "database.h"
#include "ranking.h"
#include "cJSON.h"
#include "civetweb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
static CRITICAL_SECTION g_lock;
#define LOCK_INIT() InitializeCriticalSection(&g_lock)
#define LOCK() EnterCriticalSection(&g_lock)
#define UNLOCK() LeaveCriticalSection(&g_lock)
#define LOCK_DESTROY() DeleteCriticalSection(&g_lock)
#else
#include <pthread.h>
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_INIT()
#define LOCK() pthread_mutex_lock(&g_lock)
#define UNLOCK() pthread_mutex_unlock(&g_lock)
#define LOCK_DESTROY() pthread_mutex_destroy(&g_lock)
#endif

static struct mg_context *g_mg_ctx = NULL;

/* Helper to send JSON responses with CORS headers */
static int send_json(struct mg_connection *conn, int status_code, cJSON *json_obj) {
    char *json_str = cJSON_PrintUnformatted(json_obj);
    size_t len = json_str ? strlen(json_str) : 0;

    const char *status_text = (status_code == 200) ? "OK" :
                              (status_code == 201) ? "Created" :
                              (status_code == 204) ? "No Content" :
                              (status_code == 400) ? "Bad Request" :
                              (status_code == 404) ? "Not Found" : "Internal Server Error";

    mg_printf(conn,
              "HTTP/1.1 %d %s\r\n"
              "Content-Type: application/json; charset=utf-8\r\n"
              "Content-Length: %d\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
              "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
              "Connection: close\r\n\r\n"
              "%s",
              status_code, status_text, (int)len, json_str ? json_str : "");

    if (json_str) free(json_str);
    cJSON_Delete(json_obj);
    return 1;
}

/* Handle CORS preflight requests */
static int handle_cors_options(struct mg_connection *conn) {
    mg_printf(conn,
              "HTTP/1.1 204 No Content\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
              "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n");
    return 1;
}

/* Helper to read HTTP POST body into allocated string */
static char* read_request_body(struct mg_connection *conn) {
    char buffer[4096];
    int bytes_read = mg_read(conn, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) return NULL;
    buffer[bytes_read] = '\0';
    return strdup(buffer);
}

/* GET /api/suggestions?prefix={prefix}&mode={cached|heap} */
static int handle_suggestions(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "GET") != 0) return 0;

    ServerContext *ctx = (ServerContext *)cbdata;
    char prefix[128] = "";
    char mode_str[32] = "cached";

    mg_get_var(req->query_string, req->query_string ? strlen(req->query_string) : 0, "prefix", prefix, sizeof(prefix));
    mg_get_var(req->query_string, req->query_string ? strlen(req->query_string) : 0, "mode", mode_str, sizeof(mode_str));

    RankingMode mode = (strcmp(mode_str, "heap") == 0) ? RANKING_MODE_HEAP : RANKING_MODE_CACHED;

    SuggestionItem *results = NULL;
    double latency_us = 0.0;
    size_t count = 0;

    LOCK();
    count = ranking_get_suggestions(ctx->trie, prefix, 10, mode, &results, &latency_us);
    UNLOCK();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "prefix", prefix);
    cJSON_AddStringToObject(resp, "mode", (mode == RANKING_MODE_CACHED) ? "cached" : "heap");
    cJSON_AddNumberToObject(resp, "count", (double)count);
    cJSON_AddNumberToObject(resp, "latency_us", latency_us);

    cJSON *items = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "keyword", results[i].keyword);
        cJSON_AddNumberToObject(item, "frequency", results[i].frequency);
        cJSON_AddItemToArray(items, item);
    }
    cJSON_AddItemToObject(resp, "suggestions", items);

    ranking_free_suggestions(results);
    return send_json(conn, 200, resp);
}

/* POST /api/search -> records search selection, updates frequency, Trie cache, SQLite */
static int handle_search_submit(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "POST") != 0) return 0;

    ServerContext *ctx = (ServerContext *)cbdata;
    char *body = read_request_body(conn);
    if (!body) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "error", "Missing request body");
        return send_json(conn, 400, err);
    }

    cJSON *parsed = cJSON_Parse(body);
    free(body);
    if (!parsed) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "error", "Invalid JSON");
        return send_json(conn, 400, err);
    }

    cJSON *kw_item = cJSON_GetObjectItemCaseSensitive(parsed, "keyword");
    if (!cJSON_IsString(kw_item) || (kw_item->valuestring == NULL)) {
        cJSON_Delete(parsed);
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "error", "Field 'keyword' is required");
        return send_json(conn, 400, err);
    }

    char keyword[128];
    strncpy(keyword, kw_item->valuestring, sizeof(keyword) - 1);
    keyword[sizeof(keyword) - 1] = '\0';
    cJSON_Delete(parsed);

    int new_frequency = 0;
    LOCK();
    /* 1. Persist to SQLite and append search history */
    db_record_search(keyword, &new_frequency, NULL);

    /* 2. Update in-memory Hash Table */
    hashtable_increment_frequency(ctx->hashmap, keyword, NULL, NULL);

    /* 3. Update Trie node and propagate cache up to root */
    trie_update_frequency(ctx->trie, keyword, new_frequency);
    ranking_propagate_cache_update(ctx->trie, keyword, new_frequency);
    UNLOCK();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "success", true);
    cJSON_AddStringToObject(resp, "keyword", keyword);
    cJSON_AddNumberToObject(resp, "new_frequency", new_frequency);

    return send_json(conn, 200, resp);
}

/* GET /api/popular */
static int handle_popular(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "GET") != 0) return 0;

    DBKeywordRecord *records = NULL;
    size_t count = 0;

    LOCK();
    count = db_get_popular_keywords(10, &records);
    UNLOCK();

    cJSON *resp = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", records[i].id);
        cJSON_AddStringToObject(item, "keyword", records[i].keyword);
        cJSON_AddNumberToObject(item, "frequency", records[i].frequency);
        cJSON_AddItemToArray(arr, item);
    }
    cJSON_AddItemToObject(resp, "popular", arr);
    db_free_keywords(records);

    return send_json(conn, 200, resp);
}

/* GET /api/history */
static int handle_history(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "GET") != 0) return 0;

    DBHistoryRecord *records = NULL;
    size_t count = 0;

    LOCK();
    count = db_get_recent_history(15, &records);
    UNLOCK();

    cJSON *resp = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", records[i].id);
        cJSON_AddStringToObject(item, "keyword", records[i].keyword);
        cJSON_AddStringToObject(item, "searched_at", records[i].searched_at);
        cJSON_AddItemToArray(arr, item);
    }
    cJSON_AddItemToObject(resp, "history", arr);
    db_free_history(records);

    return send_json(conn, 200, resp);
}

/* POST /api/forgotten-search */
static int handle_forgotten_search(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "POST") != 0) return 0;

    ServerContext *ctx = (ServerContext *)cbdata;
    char *body = read_request_body(conn);
    if (!body) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "error", "Missing body");
        return send_json(conn, 400, err);
    }

    cJSON *parsed = cJSON_Parse(body);
    free(body);
    if (!parsed) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "error", "Invalid JSON");
        return send_json(conn, 400, err);
    }

    cJSON *desc_item = cJSON_GetObjectItemCaseSensitive(parsed, "description");
    if (!cJSON_IsString(desc_item) || (desc_item->valuestring == NULL)) {
        cJSON_Delete(parsed);
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "error", "Field 'description' is required");
        return send_json(conn, 400, err);
    }

    ConceptSearchResult *results = NULL;
    size_t count = 0;

    LOCK();
    count = concept_index_search(ctx->concepts, desc_item->valuestring, 5, &results);
    UNLOCK();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "query", desc_item->valuestring);
    cJSON_AddNumberToObject(resp, "count", (double)count);

    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "concept_name", results[i].concept_name);
        cJSON_AddStringToObject(item, "description", results[i].description);
        cJSON_AddNumberToObject(item, "relevance_score", results[i].relevance_score);

        cJSON *kw_arr = cJSON_CreateArray();
        for (size_t k = 0; k < results[i].keyword_count; k++) {
            cJSON_AddItemToArray(kw_arr, cJSON_CreateString(results[i].related_keywords[k]));
        }
        cJSON_AddItemToObject(item, "related_keywords", kw_arr);
        cJSON_AddItemToArray(arr, item);
    }
    cJSON_AddItemToObject(resp, "results", arr);

    concept_index_free_results(results);
    cJSON_Delete(parsed);

    return send_json(conn, 200, resp);
}

/* GET /api/analytics */
static int handle_analytics(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "GET") != 0) return 0;

    ServerContext *ctx = (ServerContext *)cbdata;
    DBAnalyticsSummary summary;

    LOCK();
    db_get_analytics_summary(&summary);
    float load_factor = hashtable_get_load_factor(ctx->hashmap);
    size_t collisions = hashtable_get_collision_count(ctx->hashmap);
    size_t trie_nodes = ctx->trie->total_nodes;
    size_t trie_words = ctx->trie->total_words;
    UNLOCK();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "total_keywords", (double)summary.total_keywords);
    cJSON_AddNumberToObject(resp, "active_keywords", (double)summary.active_keywords);
    cJSON_AddNumberToObject(resp, "total_searches", (double)summary.total_searches);
    cJSON_AddNumberToObject(resp, "trie_total_words", (double)trie_words);
    cJSON_AddNumberToObject(resp, "trie_total_nodes", (double)trie_nodes);
    cJSON_AddNumberToObject(resp, "hash_load_factor", (double)load_factor);
    cJSON_AddNumberToObject(resp, "hash_collisions", (double)collisions);

    return send_json(conn, 200, resp);
}

/* GET /api/performance */
static int handle_performance(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "GET") != 0) return 0;

    ServerContext *ctx = (ServerContext *)cbdata;

    /* Measure live queries for common prefix "p" and "d" */
    SuggestionItem *res_a = NULL;
    SuggestionItem *res_b = NULL;
    double lat_a = 0.0;
    double lat_b = 0.0;

    LOCK();
    ranking_get_suggestions(ctx->trie, "p", 10, RANKING_MODE_HEAP, &res_a, &lat_a);
    ranking_get_suggestions(ctx->trie, "p", 10, RANKING_MODE_CACHED, &res_b, &lat_b);
    ranking_free_suggestions(res_a);
    ranking_free_suggestions(res_b);
    UNLOCK();

    cJSON *resp = cJSON_CreateObject();
    cJSON *comp = cJSON_CreateObject();

    cJSON *heap_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(heap_obj, "name", "Approach A: Trie + Min Heap");
    cJSON_AddStringToObject(heap_obj, "complexity", "O(L + M log K)");
    cJSON_AddNumberToObject(heap_obj, "sample_latency_us", lat_a);

    cJSON *cached_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(cached_obj, "name", "Approach B: Trie + Top-10 Cache");
    cJSON_AddStringToObject(cached_obj, "complexity", "O(L + K)");
    cJSON_AddNumberToObject(cached_obj, "sample_latency_us", lat_b);

    cJSON_AddItemToObject(comp, "approach_a", heap_obj);
    cJSON_AddItemToObject(comp, "approach_b", cached_obj);
    cJSON_AddItemToObject(resp, "comparison", comp);

    return send_json(conn, 200, resp);
}

/* POST /api/keywords (Admin Add) & GET /api/keywords (Admin List) */
static int handle_keywords(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);

    ServerContext *ctx = (ServerContext *)cbdata;

    if (strcmp(req->request_method, "GET") == 0) {
        DBKeywordRecord *records = NULL;
        LOCK();
        size_t count = db_get_all_keywords(&records);
        UNLOCK();

        cJSON *resp = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();
        for (size_t i = 0; i < count; i++) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", records[i].id);
            cJSON_AddStringToObject(item, "keyword", records[i].keyword);
            cJSON_AddNumberToObject(item, "frequency", records[i].frequency);
            cJSON_AddStringToObject(item, "created_at", records[i].created_at);
            cJSON_AddStringToObject(item, "last_searched_at", records[i].last_searched_at);
            cJSON_AddBoolToObject(item, "is_active", records[i].is_active);
            cJSON_AddItemToArray(arr, item);
        }
        cJSON_AddItemToObject(resp, "keywords", arr);
        db_free_keywords(records);
        return send_json(conn, 200, resp);
    }

    if (strcmp(req->request_method, "POST") == 0) {
        char *body = read_request_body(conn);
        if (!body) return send_json(conn, 400, cJSON_CreateString("Missing body"));

        cJSON *parsed = cJSON_Parse(body);
        free(body);
        if (!parsed) return send_json(conn, 400, cJSON_CreateString("Invalid JSON"));

        cJSON *kw = cJSON_GetObjectItemCaseSensitive(parsed, "keyword");
        cJSON *freq = cJSON_GetObjectItemCaseSensitive(parsed, "frequency");
        int frequency = (freq && cJSON_IsNumber(freq)) ? freq->valueint : 0;

        if (!cJSON_IsString(kw) || !kw->valuestring) {
            cJSON_Delete(parsed);
            return send_json(conn, 400, cJSON_CreateString("Keyword required"));
        }

        int new_id = 0;
        LOCK();
        db_add_keyword(kw->valuestring, frequency, &new_id);
        trie_insert(ctx->trie, kw->valuestring, frequency);
        hashtable_insert(ctx->hashmap, kw->valuestring, frequency, NULL, NULL, true);
        ranking_propagate_cache_update(ctx->trie, kw->valuestring, frequency);
        UNLOCK();

        cJSON_Delete(parsed);
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "success", true);
        cJSON_AddNumberToObject(resp, "id", new_id);
        return send_json(conn, 201, resp);
    }

    return 0;
}

/* POST /api/keywords/cleanup (Admin Cleanup) */
static int handle_keywords_cleanup(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "POST") != 0) return 0;

    ServerContext *ctx = (ServerContext *)cbdata;
    char **deleted_kws = NULL;
    size_t count = 0;

    LOCK();
    count = db_cleanup_expired(90, &deleted_kws);
    for (size_t i = 0; i < count; i++) {
        trie_delete(ctx->trie, deleted_kws[i]);
        hashtable_delete(ctx->hashmap, deleted_kws[i]);
        ranking_propagate_cache_deletion(ctx->trie, deleted_kws[i]);
        free(deleted_kws[i]);
    }
    if (deleted_kws) free(deleted_kws);
    UNLOCK();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "success", true);
    cJSON_AddNumberToObject(resp, "cleaned_count", (double)count);

    return send_json(conn, 200, resp);
}

/* Initialize server context, databases, and in-memory structures */
bool api_server_init(ServerContext *ctx, int port, const char *db_path, const char *sample_csv_path) {
    if (!ctx) return false;

    LOCK_INIT();
    ctx->port = port;
    strncpy(ctx->db_path, db_path ? db_path : "intellisuggest.db", sizeof(ctx->db_path) - 1);

    ctx->trie = trie_create();
    ctx->hashmap = hashtable_create(128);
    ctx->concepts = concept_index_create();

    if (!db_init(ctx->db_path)) {
        fprintf(stderr, "[Server] Failed to initialize SQLite database at %s\n", ctx->db_path);
        return false;
    }

    /* Seed from CSV if provided and DB empty */
    if (sample_csv_path) {
        db_seed_from_csv(sample_csv_path);
    }

    /* Hydrate in-memory Trie and Hash Table from DB */
    size_t loaded = db_load_keywords_into_memory(ctx->trie, ctx->hashmap);
    printf("[Server] Hydrated %zu keywords into in-memory Trie and Hash Table.\n", loaded);

    /* Build initial Top-10 caches */
    ranking_rebuild_entire_cache(ctx->trie);
    printf("[Server] Precomputed Top-10 node caches in Trie.\n");

    /* Seed default concepts into concept inverted index */
    concept_index_seed_defaults(ctx->concepts);
    printf("[Server] Indexed %zu concepts in Forgotten-Name inverted index.\n", ctx->concepts->concept_count);

    return true;
}

/* Start CivetWeb HTTP daemon */
void api_server_start(ServerContext *ctx) {
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", ctx->port);

    const char *options[] = {
        "listening_ports", port_str,
        "num_threads", "10",
        "document_root", "frontend/dist",
        NULL
    };

    g_mg_ctx = mg_start(NULL, NULL, options);
    if (!g_mg_ctx) {
        fprintf(stderr, "[Server Error] Failed to start CivetWeb on port %d\n", ctx->port);
        return;
    }

    /* Register REST handlers */
    mg_set_request_handler(g_mg_ctx, "/api/suggestions", handle_suggestions, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/search", handle_search_submit, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/popular", handle_popular, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/history", handle_history, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/forgotten-search", handle_forgotten_search, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/analytics", handle_analytics, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/performance", handle_performance, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/keywords/cleanup", handle_keywords_cleanup, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/keywords", handle_keywords, ctx);

    printf("======================================================================\n");
    printf("   INTELLISUGGEST CIVETWEB HTTP REST API RUNNING ON PORT %d\n", ctx->port);
    printf("   Available Endpoints:\n");
    printf("     GET  /api/suggestions?prefix=...&mode=cached|heap\n");
    printf("     POST /api/search           (body: {\"keyword\": \"...\"})\n");
    printf("     GET  /api/popular\n");
    printf("     GET  /api/history\n");
    printf("     POST /api/forgotten-search (body: {\"description\": \"...\"})\n");
    printf("     GET  /api/analytics\n");
    printf("     GET  /api/performance\n");
    printf("     GET  /api/keywords\n");
    printf("     POST /api/keywords\n");
    printf("     POST /api/keywords/cleanup\n");
    printf("======================================================================\n");
}

void api_server_stop(void) {
    if (g_mg_ctx) {
        mg_stop(g_mg_ctx);
        g_mg_ctx = NULL;
    }
}

void api_server_cleanup(ServerContext *ctx) {
    api_server_stop();
    if (ctx) {
        if (ctx->trie) { trie_free(ctx->trie); ctx->trie = NULL; }
        if (ctx->hashmap) { hashtable_free(ctx->hashmap); ctx->hashmap = NULL; }
        if (ctx->concepts) { concept_index_free(ctx->concepts); ctx->concepts = NULL; }
    }
    db_close();
    LOCK_DESTROY();
}
