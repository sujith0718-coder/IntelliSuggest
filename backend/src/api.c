#include "api.h"
#include "database.h"
#include "ranking.h"
#include "concept_index.h"
#include "cJSON.h"
#include "civetweb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
static CRITICAL_SECTION g_lock;
#define LOCK_INIT() InitializeCriticalSection(&g_lock)
#define LOCK() EnterCriticalSection(&g_lock)
#define UNLOCK() LeaveCriticalSection(&g_lock)
#define LOCK_DESTROY() DeleteCriticalSection(&g_lock)
#else
#include <pthread.h>
#include <unistd.h>
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_INIT()
#define LOCK() pthread_mutex_lock(&g_lock)
#define UNLOCK() pthread_mutex_unlock(&g_lock)
#define LOCK_DESTROY() pthread_mutex_destroy(&g_lock)
#endif

static struct mg_context *g_mg_ctx = NULL;

/* -------------------------------------------------------------------------
 * Search Persistence Queue (Reliable Asynchronous Storage)
 * ------------------------------------------------------------------------- */
typedef struct {
    char id[64];
    char keyword[128];
    char searched_at[32];
    int retries;
} SearchTask;

#define PERSISTENCE_QUEUE_SIZE 1024

typedef struct {
    SearchTask tasks[PERSISTENCE_QUEUE_SIZE];
    size_t head;
    size_t tail;
    size_t count;
    size_t pending_writes;
    size_t persisted_writes;
    size_t failed_writes;
    bool is_running;
    ServerContext *ctx;
#if defined(_WIN32)
    CRITICAL_SECTION lock;
    HANDLE event;
    HANDLE thread;
#else
    pthread_mutex_t lock;
    pthread_cond_t cond;
    pthread_t thread;
#endif
} PersistenceQueue;

static PersistenceQueue g_queue;

#if defined(_WIN32)
static DWORD WINAPI persistence_worker_func(LPVOID arg) {
    (void)arg;
#else
static void* persistence_worker_func(void *arg) {
    (void)arg;
#endif
    while (g_queue.is_running || g_queue.count > 0) {
#if defined(_WIN32)
        WaitForSingleObject(g_queue.event, 250);
        EnterCriticalSection(&g_queue.lock);
#else
        pthread_mutex_lock(&g_queue.lock);
        while (g_queue.count == 0 && g_queue.is_running) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 250000000;
            if (ts.tv_nsec >= 1000000000) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000; }
            pthread_cond_timedwait(&g_queue.cond, &g_queue.lock, &ts);
        }
#endif
        if (g_queue.count == 0) {
#if defined(_WIN32)
            LeaveCriticalSection(&g_queue.lock);
#else
            pthread_mutex_unlock(&g_queue.lock);
#endif
            continue;
        }

        SearchTask task = g_queue.tasks[g_queue.head];
        g_queue.head = (g_queue.head + 1) % PERSISTENCE_QUEUE_SIZE;
        g_queue.count--;
#if defined(_WIN32)
        LeaveCriticalSection(&g_queue.lock);
#else
        pthread_mutex_unlock(&g_queue.lock);
#endif

        int out_freq = 0;
        bool ok = db_record_search(task.keyword, &out_freq, task.searched_at);

#if defined(_WIN32)
        EnterCriticalSection(&g_queue.lock);
#else
        pthread_mutex_lock(&g_queue.lock);
#endif
        if (ok) {
            g_queue.persisted_writes++;
            if (g_queue.pending_writes > 0) g_queue.pending_writes--;
#if defined(_WIN32)
            LeaveCriticalSection(&g_queue.lock);
#else
            pthread_mutex_unlock(&g_queue.lock);
#endif
        } else {
            if (task.retries < 3) {
                task.retries++;
#if defined(_WIN32)
                LeaveCriticalSection(&g_queue.lock);
                Sleep(200);
                EnterCriticalSection(&g_queue.lock);
#else
                pthread_mutex_unlock(&g_queue.lock);
                usleep(200000);
                pthread_mutex_lock(&g_queue.lock);
#endif
                if (g_queue.count < PERSISTENCE_QUEUE_SIZE) {
                    g_queue.tasks[g_queue.tail] = task;
                    g_queue.tail = (g_queue.tail + 1) % PERSISTENCE_QUEUE_SIZE;
                    g_queue.count++;
                }
#if defined(_WIN32)
                LeaveCriticalSection(&g_queue.lock);
#else
                pthread_mutex_unlock(&g_queue.lock);
#endif
            } else {
                g_queue.failed_writes++;
                if (g_queue.pending_writes > 0) g_queue.pending_writes--;
#if defined(_WIN32)
                LeaveCriticalSection(&g_queue.lock);
#else
                pthread_mutex_unlock(&g_queue.lock);
#endif
                fprintf(stderr, "[Persistence Warning] Failed to persist search task '%s' for '%s' after 3 retries\n",
                        task.id, task.keyword);
            }
        }
    }
#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

static void persistence_queue_init(ServerContext *ctx) {
    memset(&g_queue, 0, sizeof(g_queue));
    g_queue.ctx = ctx;
    g_queue.is_running = true;
#if defined(_WIN32)
    InitializeCriticalSection(&g_queue.lock);
    g_queue.event = CreateEvent(NULL, FALSE, FALSE, NULL);
    g_queue.thread = CreateThread(NULL, 0, persistence_worker_func, NULL, 0, NULL);
#else
    pthread_mutex_init(&g_queue.lock, NULL);
    pthread_cond_init(&g_queue.cond, NULL);
    pthread_create(&g_queue.thread, NULL, persistence_worker_func, NULL);
#endif
}

static void persistence_queue_stop(void) {
    g_queue.is_running = false;
#if defined(_WIN32)
    SetEvent(g_queue.event);
    WaitForSingleObject(g_queue.thread, 3000);
    CloseHandle(g_queue.thread);
    CloseHandle(g_queue.event);
    DeleteCriticalSection(&g_queue.lock);
#else
    pthread_cond_signal(&g_queue.cond);
    pthread_join(g_queue.thread, NULL);
    pthread_cond_destroy(&g_queue.cond);
    pthread_mutex_destroy(&g_queue.lock);
#endif
}

static bool persistence_queue_push(const char *keyword, const char *searched_at, char *out_event_id, size_t id_len) {
    if (!keyword) return false;
    static uint64_t counter = 0;
    char evt_id[64];
    snprintf(evt_id, sizeof(evt_id), "evt_%llu_%u", (unsigned long long)time(NULL), (unsigned int)(++counter));
    if (out_event_id && id_len > 0) {
        strncpy(out_event_id, evt_id, id_len - 1);
        out_event_id[id_len - 1] = '\0';
    }

#if defined(_WIN32)
    EnterCriticalSection(&g_queue.lock);
#else
    pthread_mutex_lock(&g_queue.lock);
#endif
    if (g_queue.count >= PERSISTENCE_QUEUE_SIZE) {
#if defined(_WIN32)
        LeaveCriticalSection(&g_queue.lock);
#else
        pthread_mutex_unlock(&g_queue.lock);
#endif
        return false;
    }

    SearchTask *t = &g_queue.tasks[g_queue.tail];
    strncpy(t->id, evt_id, sizeof(t->id) - 1);
    t->id[sizeof(t->id) - 1] = '\0';
    strncpy(t->keyword, keyword, sizeof(t->keyword) - 1);
    t->keyword[sizeof(t->keyword) - 1] = '\0';

    if (searched_at && strlen(searched_at) > 0) {
        strncpy(t->searched_at, searched_at, sizeof(t->searched_at) - 1);
        t->searched_at[sizeof(t->searched_at) - 1] = '\0';
    } else {
        time_t now = time(NULL);
        struct tm *tm = gmtime(&now);
        if (tm) strftime(t->searched_at, sizeof(t->searched_at), "%Y-%m-%dT%H:%M:%SZ", tm);
        else snprintf(t->searched_at, sizeof(t->searched_at), "1970-01-01T00:00:00Z");
    }
    t->retries = 0;

    g_queue.tail = (g_queue.tail + 1) % PERSISTENCE_QUEUE_SIZE;
    g_queue.count++;
    g_queue.pending_writes++;

#if defined(_WIN32)
    SetEvent(g_queue.event);
    LeaveCriticalSection(&g_queue.lock);
#else
    pthread_cond_signal(&g_queue.cond);
    pthread_mutex_unlock(&g_queue.lock);
#endif
    return true;
}

/* -------------------------------------------------------------------------
 * Security & Administrative Authentication
 * ------------------------------------------------------------------------- */
static bool check_admin_auth(struct mg_connection *conn) {
    const char *expected_key = getenv("ADMIN_API_KEY");
    if (!expected_key || strlen(expected_key) == 0) {
        /* Fail-closed: administrative operations disabled if key is not configured */
        return false;
    }
    const char *auth_hdr = mg_get_header(conn, "Authorization");
    if (!auth_hdr || strncmp(auth_hdr, "Bearer ", 7) != 0) return false;
    const char *provided_key = auth_hdr + 7;

    size_t len_a = strlen(expected_key);
    size_t len_b = strlen(provided_key);
    if (len_a != len_b) return false;

    /* Constant-time comparison to prevent timing side-channel attacks */
    unsigned char diff = 0;
    for (size_t i = 0; i < len_a; i++) {
        diff |= (unsigned char)(expected_key[i] ^ provided_key[i]);
    }
    return (diff == 0);
}

/* Helper to resolve allowed CORS origin */
static void get_cors_origin(struct mg_connection *conn, char *buf, size_t buf_len) {
    const char *env_origin = getenv("CORS_ORIGIN");
    if (env_origin && strlen(env_origin) > 0) {
        strncpy(buf, env_origin, buf_len - 1);
        buf[buf_len - 1] = '\0';
        return;
    }
    const char *req_origin = mg_get_header(conn, "Origin");
    if (req_origin && strlen(req_origin) > 0) {
        strncpy(buf, req_origin, buf_len - 1);
        buf[buf_len - 1] = '\0';
        return;
    }
    strncpy(buf, "*", buf_len - 1);
    buf[buf_len - 1] = '\0';
}

/* Helper to send JSON responses with CORS headers */
static int send_json(struct mg_connection *conn, int status_code, cJSON *json_obj) {
    char *json_str = cJSON_PrintUnformatted(json_obj);
    size_t len = json_str ? strlen(json_str) : 0;

    const char *status_text = (status_code == 200) ? "OK" :
                              (status_code == 201) ? "Created" :
                              (status_code == 204) ? "No Content" :
                              (status_code == 400) ? "Bad Request" :
                              (status_code == 401) ? "Unauthorized" :
                              (status_code == 403) ? "Forbidden" :
                              (status_code == 404) ? "Not Found" : "Internal Server Error";

    char cors_origin[256];
    get_cors_origin(conn, cors_origin, sizeof(cors_origin));

    mg_printf(conn,
              "HTTP/1.1 %d %s\r\n"
              "Content-Type: application/json; charset=utf-8\r\n"
              "Content-Length: %d\r\n"
              "Access-Control-Allow-Origin: %s\r\n"
              "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
              "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n"
              "Access-Control-Allow-Credentials: true\r\n"
              "Connection: close\r\n\r\n"
              "%s",
              status_code, status_text, (int)len, cors_origin, json_str ? json_str : "");

    if (json_str) free(json_str);
    cJSON_Delete(json_obj);
    return 1;
}

/* Handle CORS preflight requests */
static int handle_cors_options(struct mg_connection *conn) {
    char cors_origin[256];
    get_cors_origin(conn, cors_origin, sizeof(cors_origin));

    mg_printf(conn,
              "HTTP/1.1 204 No Content\r\n"
              "Access-Control-Allow-Origin: %s\r\n"
              "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
              "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n"
              "Access-Control-Allow-Credentials: true\r\n"
              "Content-Length: 0\r\n"
              "Connection: close\r\n\r\n",
              cors_origin);
    return 1;
}

/* Helper to read HTTP request body safely */
static char* read_request_body(struct mg_connection *conn) {
    char buffer[4096];
    int bytes_read = mg_read(conn, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) return NULL;
    buffer[bytes_read] = '\0';
    return strdup(buffer);
}

/* GET /api/health and /health */
static int handle_health(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    cJSON_AddStringToObject(resp, "service", "IntelliSuggest C DSA Core");
    cJSON_AddStringToObject(resp, "database_driver", db_get_driver_name());
    cJSON_AddStringToObject(resp, "database", "connected");
    cJSON_AddStringToObject(resp, "persistence", (g_queue.failed_writes > 0) ? "degraded" : "healthy");
    cJSON_AddNumberToObject(resp, "pending_writes", (double)g_queue.pending_writes);
    cJSON_AddNumberToObject(resp, "persisted_writes", (double)g_queue.persisted_writes);
    cJSON_AddNumberToObject(resp, "failed_writes", (double)g_queue.failed_writes);
    return send_json(conn, 200, resp);
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

/* POST /api/search -> updates memory immediately & queues durable async persistence */
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
        cJSON_AddStringToObject(err, "error", "Invalid JSON format");
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

    /* Normalize keyword */
    char *norm = trie_normalize(keyword);
    if (!norm || strlen(norm) == 0) {
        if (norm) free(norm);
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "error", "Keyword must contain alphabetic characters");
        return send_json(conn, 400, err);
    }

    int new_frequency = 0;
    LOCK();
    /* 1. Update in-memory Hash Table */
    hashtable_increment_frequency(ctx->hashmap, norm, NULL, &new_frequency);
    if (new_frequency <= 0) {
        HashNode *hn = hashtable_search(ctx->hashmap, norm);
        new_frequency = hn ? hn->frequency : 1;
    }

    /* 2. Update Trie node and propagate cache up to root */
    trie_update_frequency(ctx->trie, norm, new_frequency);
    ranking_propagate_cache_update(ctx->trie, norm, new_frequency);
    UNLOCK();

    /* 3. Queue durable persistence asynchronously */
    char event_id[64] = {0};
    bool queued = persistence_queue_push(norm, NULL, event_id, sizeof(event_id));

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "success", true);
    cJSON_AddStringToObject(resp, "keyword", norm);
    cJSON_AddNumberToObject(resp, "new_frequency", new_frequency);
    cJSON_AddStringToObject(resp, "event_id", event_id);
    cJSON_AddStringToObject(resp, "persistence_status", queued ? "queued" : "queue_full");

    free(norm);
    return send_json(conn, 200, resp);
}

/* GET /api/popular */
static int handle_popular(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "GET") != 0) return 0;

    DBKeywordRecord *records = NULL;
    LOCK();
    size_t count = db_get_popular_keywords(10, &records);
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
    (void)cbdata;
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "GET") != 0) return 0;

    DBHistoryRecord *records = NULL;
    LOCK();
    size_t count = db_get_recent_history(15, &records);
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
    cJSON_AddNumberToObject(resp, "expired_keywords", (double)summary.expired_keywords);
    cJSON_AddNumberToObject(resp, "total_searches", (double)summary.total_searches);
    cJSON_AddNumberToObject(resp, "trie_total_words", (double)trie_words);
    cJSON_AddNumberToObject(resp, "trie_total_nodes", (double)trie_nodes);
    cJSON_AddNumberToObject(resp, "hash_load_factor", (double)load_factor);
    cJSON_AddNumberToObject(resp, "hash_collisions", (double)collisions);
    cJSON_AddNumberToObject(resp, "pending_writes", (double)g_queue.pending_writes);
    cJSON_AddNumberToObject(resp, "persisted_writes", (double)g_queue.persisted_writes);
    cJSON_AddNumberToObject(resp, "failed_writes", (double)g_queue.failed_writes);

    return send_json(conn, 200, resp);
}

/* GET /api/analytics/trending */
static int handle_analytics_trending(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "GET") != 0) return 0;

    DBKeywordRecord *records = NULL;
    LOCK();
    size_t count = db_get_trending_keywords(10, 24, &records);
    UNLOCK();

    cJSON *resp = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", records[i].id);
        cJSON_AddStringToObject(item, "keyword", records[i].keyword);
        cJSON_AddNumberToObject(item, "frequency", records[i].frequency);
        cJSON_AddStringToObject(item, "last_searched_at", records[i].last_searched_at);
        cJSON_AddItemToArray(arr, item);
    }
    cJSON_AddItemToObject(resp, "trending", arr);
    db_free_keywords(records);

    return send_json(conn, 200, resp);
}

/* GET /api/analytics/expired */
static int handle_analytics_expired(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "GET") != 0) return 0;

    DBKeywordRecord *records = NULL;
    LOCK();
    size_t count = db_get_expired_keywords(90, &records);
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
    cJSON_AddItemToObject(resp, "expired", arr);
    db_free_keywords(records);

    return send_json(conn, 200, resp);
}

/* GET /api/analytics/recent */
static int handle_analytics_recent(struct mg_connection *conn, void *cbdata) {
    return handle_history(conn, cbdata);
}

/* GET /api/performance */
static int handle_performance(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "GET") != 0) return 0;

    ServerContext *ctx = (ServerContext *)cbdata;

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

/* /api/keywords: GET (list), POST (add), PUT (update), DELETE (delete) */
static int handle_keywords(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);

    ServerContext *ctx = (ServerContext *)cbdata;

    /* Public GET */
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

    /* Modifying Operations Require Admin Auth */
    if (!check_admin_auth(conn)) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "error", "Unauthorized: Valid ADMIN_API_KEY Bearer token required");
        return send_json(conn, 401, err);
    }

    /* POST /api/keywords (Add) */
    if (strcmp(req->request_method, "POST") == 0) {
        char *body = read_request_body(conn);
        if (!body) return send_json(conn, 400, cJSON_CreateString("Missing body"));

        cJSON *parsed = cJSON_Parse(body);
        free(body);
        if (!parsed) return send_json(conn, 400, cJSON_CreateString("Invalid JSON"));

        cJSON *kw = cJSON_GetObjectItemCaseSensitive(parsed, "keyword");
        cJSON *freq = cJSON_GetObjectItemCaseSensitive(parsed, "frequency");
        int frequency = (freq && cJSON_IsNumber(freq)) ? freq->valueint : 10;

        if (!cJSON_IsString(kw) || !kw->valuestring) {
            cJSON_Delete(parsed);
            return send_json(conn, 400, cJSON_CreateString("Field 'keyword' is required"));
        }

        char *norm = trie_normalize(kw->valuestring);
        if (!norm || strlen(norm) == 0) {
            if (norm) free(norm);
            cJSON_Delete(parsed);
            return send_json(conn, 400, cJSON_CreateString("Keyword must contain alphabetic characters"));
        }

        int new_id = 0;
        LOCK();
        db_add_keyword(norm, frequency, &new_id);
        trie_insert(ctx->trie, norm, frequency);
        hashtable_insert(ctx->hashmap, norm, frequency, NULL, NULL, true);
        ranking_propagate_cache_update(ctx->trie, norm, frequency);
        UNLOCK();

        free(norm);
        cJSON_Delete(parsed);
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "success", true);
        cJSON_AddNumberToObject(resp, "id", new_id);
        return send_json(conn, 201, resp);
    }

    /* PUT /api/keywords (Update) */
    if (strcmp(req->request_method, "PUT") == 0) {
        char *body = read_request_body(conn);
        if (!body) return send_json(conn, 400, cJSON_CreateString("Missing body"));

        cJSON *parsed = cJSON_Parse(body);
        free(body);
        if (!parsed) return send_json(conn, 400, cJSON_CreateString("Invalid JSON"));

        cJSON *id_item = cJSON_GetObjectItemCaseSensitive(parsed, "id");
        cJSON *kw_item = cJSON_GetObjectItemCaseSensitive(parsed, "keyword");
        cJSON *freq_item = cJSON_GetObjectItemCaseSensitive(parsed, "frequency");
        cJSON *act_item = cJSON_GetObjectItemCaseSensitive(parsed, "is_active");

        if (!cJSON_IsNumber(id_item) || !cJSON_IsString(kw_item)) {
            cJSON_Delete(parsed);
            return send_json(conn, 400, cJSON_CreateString("Fields 'id' and 'keyword' are required"));
        }

        int id = id_item->valueint;
        int frequency = freq_item ? freq_item->valueint : 1;
        bool is_active = act_item ? cJSON_IsTrue(act_item) : true;

        char *norm = trie_normalize(kw_item->valuestring);
        if (!norm) {
            cJSON_Delete(parsed);
            return send_json(conn, 400, cJSON_CreateString("Invalid keyword"));
        }

        LOCK();
        db_update_keyword(id, norm, frequency, is_active);
        if (is_active) {
            trie_insert(ctx->trie, norm, frequency);
            hashtable_insert(ctx->hashmap, norm, frequency, NULL, NULL, true);
            ranking_propagate_cache_update(ctx->trie, norm, frequency);
        } else {
            trie_delete(ctx->trie, norm);
            hashtable_delete(ctx->hashmap, norm);
            ranking_propagate_cache_deletion(ctx->trie, norm);
        }
        UNLOCK();

        free(norm);
        cJSON_Delete(parsed);
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "success", true);
        return send_json(conn, 200, resp);
    }

    /* DELETE /api/keywords (Delete by id or keyword) */
    if (strcmp(req->request_method, "DELETE") == 0) {
        int id = 0;
        char id_str[32] = "";
        if (req->query_string) {
            mg_get_var(req->query_string, strlen(req->query_string), "id", id_str, sizeof(id_str));
            if (strlen(id_str) > 0) id = atoi(id_str);
        }

        char *body = read_request_body(conn);
        if (body) {
            cJSON *parsed = cJSON_Parse(body);
            free(body);
            if (parsed) {
                cJSON *id_item = cJSON_GetObjectItemCaseSensitive(parsed, "id");
                if (cJSON_IsNumber(id_item)) id = id_item->valueint;
                cJSON_Delete(parsed);
            }
        }

        if (id <= 0) {
            return send_json(conn, 400, cJSON_CreateString("Valid keyword 'id' required"));
        }

        char deleted_kw[128] = {0};
        LOCK();
        bool ok = db_delete_keyword(id, deleted_kw, sizeof(deleted_kw));
        if (ok && strlen(deleted_kw) > 0) {
            trie_delete(ctx->trie, deleted_kw);
            hashtable_delete(ctx->hashmap, deleted_kw);
            ranking_propagate_cache_deletion(ctx->trie, deleted_kw);
        }
        UNLOCK();

        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "success", ok);
        cJSON_AddStringToObject(resp, "deleted_keyword", deleted_kw);
        return send_json(conn, 200, resp);
    }

    return 0;
}

/* POST /api/keywords/cleanup (Admin Cleanup) */
static int handle_keywords_cleanup(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);
    if (strcmp(req->request_method, "POST") != 0) return 0;

    if (!check_admin_auth(conn)) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "error", "Unauthorized: Valid ADMIN_API_KEY Bearer token required");
        return send_json(conn, 401, err);
    }

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

/* /api/concepts: GET (list/get), POST (create), PUT (update), DELETE (delete) */
static int handle_concepts(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req = mg_get_request_info(conn);
    if (strcmp(req->request_method, "OPTIONS") == 0) return handle_cors_options(conn);

    ServerContext *ctx = (ServerContext *)cbdata;

    /* Public GET */
    if (strcmp(req->request_method, "GET") == 0) {
        char id_str[32] = "";
        if (req->query_string) {
            mg_get_var(req->query_string, strlen(req->query_string), "id", id_str, sizeof(id_str));
        }

        if (strlen(id_str) > 0) {
            int cid = atoi(id_str);
            DBConceptRecord rec;
            LOCK();
            bool found = db_get_concept_by_id(cid, &rec);
            UNLOCK();

            if (!found) return send_json(conn, 404, cJSON_CreateString("Concept not found"));

            cJSON *resp = cJSON_CreateObject();
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", rec.id);
            cJSON_AddStringToObject(item, "concept_name", rec.concept_name);
            cJSON_AddStringToObject(item, "description", rec.description);
            cJSON_AddStringToObject(item, "keywords", rec.keywords);
            cJSON_AddItemToObject(resp, "concept", item);
            return send_json(conn, 200, resp);
        }

        DBConceptRecord *records = NULL;
        LOCK();
        size_t count = db_get_all_concepts(&records);
        UNLOCK();

        cJSON *resp = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();
        for (size_t i = 0; i < count; i++) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", records[i].id);
            cJSON_AddStringToObject(item, "concept_name", records[i].concept_name);
            cJSON_AddStringToObject(item, "description", records[i].description);
            cJSON_AddStringToObject(item, "keywords", records[i].keywords);
            cJSON_AddItemToArray(arr, item);
        }
        cJSON_AddItemToObject(resp, "concepts", arr);
        db_free_concepts(records);
        return send_json(conn, 200, resp);
    }

    /* Modifying Operations Require Admin Auth */
    if (!check_admin_auth(conn)) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "error", "Unauthorized: Valid ADMIN_API_KEY Bearer token required");
        return send_json(conn, 401, err);
    }

    /* POST /api/concepts (Create) */
    if (strcmp(req->request_method, "POST") == 0) {
        char *body = read_request_body(conn);
        if (!body) return send_json(conn, 400, cJSON_CreateString("Missing body"));

        cJSON *parsed = cJSON_Parse(body);
        free(body);
        if (!parsed) return send_json(conn, 400, cJSON_CreateString("Invalid JSON"));

        cJSON *nm = cJSON_GetObjectItemCaseSensitive(parsed, "concept_name");
        cJSON *ds = cJSON_GetObjectItemCaseSensitive(parsed, "description");
        cJSON *kw = cJSON_GetObjectItemCaseSensitive(parsed, "keywords");

        if (!cJSON_IsString(nm) || !cJSON_IsString(ds)) {
            cJSON_Delete(parsed);
            return send_json(conn, 400, cJSON_CreateString("Fields 'concept_name' and 'description' are required"));
        }

        const char *kws_str = (kw && cJSON_IsString(kw)) ? kw->valuestring : "";
        int new_id = 0;

        LOCK();
        bool ok = db_add_concept(nm->valuestring, ds->valuestring, kws_str, &new_id);
        if (ok && new_id > 0) {
            concept_index_add(ctx->concepts, new_id, nm->valuestring, ds->valuestring, kws_str);
        }
        UNLOCK();

        cJSON_Delete(parsed);
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "success", ok);
        cJSON_AddNumberToObject(resp, "id", new_id);
        return send_json(conn, 201, resp);
    }

    /* PUT /api/concepts (Update) */
    if (strcmp(req->request_method, "PUT") == 0) {
        char *body = read_request_body(conn);
        if (!body) return send_json(conn, 400, cJSON_CreateString("Missing body"));

        cJSON *parsed = cJSON_Parse(body);
        free(body);
        if (!parsed) return send_json(conn, 400, cJSON_CreateString("Invalid JSON"));

        cJSON *id_item = cJSON_GetObjectItemCaseSensitive(parsed, "id");
        cJSON *nm = cJSON_GetObjectItemCaseSensitive(parsed, "concept_name");
        cJSON *ds = cJSON_GetObjectItemCaseSensitive(parsed, "description");
        cJSON *kw = cJSON_GetObjectItemCaseSensitive(parsed, "keywords");

        if (!cJSON_IsNumber(id_item) || !cJSON_IsString(nm) || !cJSON_IsString(ds)) {
            cJSON_Delete(parsed);
            return send_json(conn, 400, cJSON_CreateString("Fields 'id', 'concept_name', and 'description' are required"));
        }

        int id = id_item->valueint;
        const char *kws_str = (kw && cJSON_IsString(kw)) ? kw->valuestring : "";

        LOCK();
        bool ok = db_update_concept(id, nm->valuestring, ds->valuestring, kws_str);
        if (ok) {
            concept_index_update(ctx->concepts, id, nm->valuestring, ds->valuestring, kws_str);
        }
        UNLOCK();

        cJSON_Delete(parsed);
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "success", ok);
        return send_json(conn, 200, resp);
    }

    /* DELETE /api/concepts (Delete) */
    if (strcmp(req->request_method, "DELETE") == 0) {
        int id = 0;
        char id_str[32] = "";
        if (req->query_string) {
            mg_get_var(req->query_string, strlen(req->query_string), "id", id_str, sizeof(id_str));
            if (strlen(id_str) > 0) id = atoi(id_str);
        }

        char *body = read_request_body(conn);
        if (body) {
            cJSON *parsed = cJSON_Parse(body);
            free(body);
            if (parsed) {
                cJSON *id_item = cJSON_GetObjectItemCaseSensitive(parsed, "id");
                if (cJSON_IsNumber(id_item)) id = id_item->valueint;
                cJSON_Delete(parsed);
            }
        }

        if (id <= 0) {
            return send_json(conn, 400, cJSON_CreateString("Valid concept 'id' required"));
        }

        LOCK();
        bool ok = db_delete_concept(id);
        if (ok) {
            concept_index_delete(ctx->concepts, id);
        }
        UNLOCK();

        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "success", ok);
        return send_json(conn, 200, resp);
    }

    return 0;
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

    /* Hydrate concepts into concept inverted index from DB */
    size_t concepts_loaded = db_load_concepts_into_memory(ctx->concepts);
    printf("[Server] Hydrated %zu concepts into Forgotten-Name inverted index from database.\n", concepts_loaded);

    /* Initialize persistence worker queue */
    persistence_queue_init(ctx);
    printf("[Server] Started reliable search persistence queue worker.\n");

    return true;
}

/* Start CivetWeb HTTP daemon */
void api_server_start(ServerContext *ctx) {
    char port_str[32];
    snprintf(port_str, sizeof(port_str), "0.0.0.0:%d", ctx->port);

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
    mg_set_request_handler(g_mg_ctx, "/api/health", handle_health, ctx);
    mg_set_request_handler(g_mg_ctx, "/health", handle_health, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/suggestions", handle_suggestions, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/search", handle_search_submit, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/popular", handle_popular, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/history", handle_history, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/forgotten-search", handle_forgotten_search, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/analytics/trending", handle_analytics_trending, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/analytics/expired", handle_analytics_expired, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/analytics/recent", handle_analytics_recent, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/analytics", handle_analytics, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/performance", handle_performance, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/keywords/cleanup", handle_keywords_cleanup, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/keywords", handle_keywords, ctx);
    mg_set_request_handler(g_mg_ctx, "/api/concepts", handle_concepts, ctx);

    printf("======================================================================\n");
    printf("   INTELLISUGGEST CIVETWEB HTTP REST API RUNNING ON PORT %d\n", ctx->port);
    printf("   Available Endpoints:\n");
    printf("     GET  /api/suggestions?prefix=...&mode=cached|heap\n");
    printf("     POST /api/search           (body: {\"keyword\": \"...\"})\n");
    printf("     GET  /api/popular\n");
    printf("     GET  /api/history\n");
    printf("     POST /api/forgotten-search (body: {\"description\": \"...\"})\n");
    printf("     GET  /api/concepts         (dynamic concepts)\n");
    printf("     POST /api/concepts         (ADMIN: create concept)\n");
    printf("     PUT  /api/concepts         (ADMIN: update concept)\n");
    printf("     DELETE /api/concepts       (ADMIN: delete concept)\n");
    printf("     GET  /api/analytics\n");
    printf("     GET  /api/analytics/trending\n");
    printf("     GET  /api/analytics/expired\n");
    printf("     GET  /api/performance\n");
    printf("     GET  /api/keywords\n");
    printf("     POST /api/keywords         (ADMIN: add keyword)\n");
    printf("     PUT  /api/keywords         (ADMIN: update keyword)\n");
    printf("     DELETE /api/keywords       (ADMIN: delete keyword)\n");
    printf("     POST /api/keywords/cleanup (ADMIN: cleanup expired)\n");
    printf("======================================================================\n");
}

/* Stop CivetWeb HTTP daemon */
void api_server_stop(void) {
    persistence_queue_stop();
    if (g_mg_ctx) {
        mg_stop(g_mg_ctx);
        g_mg_ctx = NULL;
    }
}

/* Free server context and close database */
void api_server_cleanup(ServerContext *ctx) {
    if (!ctx) return;
    if (ctx->trie) { trie_free(ctx->trie); ctx->trie = NULL; }
    if (ctx->hashmap) { hashtable_free(ctx->hashmap); ctx->hashmap = NULL; }
    if (ctx->concepts) { concept_index_free(ctx->concepts); ctx->concepts = NULL; }
    db_close();
    LOCK_DESTROY();
}
