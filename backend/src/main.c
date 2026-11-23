/**
 * ============================================================================
 * PROJECT: INTELLISUGGEST
 * A High-Performance, Frequency-Ranked Autocomplete and Search Suggestion Engine
 * ----------------------------------------------------------------------------
 * Academic Data Structures and Algorithms Project - Team 12
 * ============================================================================
 */

#include "api.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#if defined(_WIN32)
#include <windows.h>
#include <direct.h>
#define sleep_sec(s) Sleep((s) * 1000)
#define make_dir(p) _mkdir(p)
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#define sleep_sec(s) sleep(s)
#define make_dir(p) mkdir(p, 0777)
#endif

static volatile int g_running = 1;

static void ensure_parent_dir(const char *path) {
    if (!path) return;
    char temp[512];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    char *last_slash = strrchr(temp, '/');
#if defined(_WIN32)
    char *last_bslash = strrchr(temp, '\\');
    if (last_bslash && (!last_slash || last_bslash > last_slash)) {
        last_slash = last_bslash;
    }
#endif
    if (last_slash) {
        *last_slash = '\0';
        for (char *p = temp + 1; *p; p++) {
            if (*p == '/' || *p == '\\') {
                *p = '\0';
                make_dir(temp);
                *p = '/';
            }
        }
        make_dir(temp);
    }
}

static void handle_signal(int sig) {
    (void)sig;
    printf("\n[Server] Shutdown signal received. Exiting gracefully...\n");
    g_running = 0;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("======================================================================\n");
    printf("               INTELLISUGGEST CORE ENGINE (C / DSA)                   \n");
    printf("    A High-Performance Frequency-Ranked Search Suggestion Engine      \n");
    printf("======================================================================\n");
    printf("Academic Project - Team 12:\n");
    printf("  [25Z207] Aravind Deva M       - Trie & Prefix Searching\n");
    printf("  [25Z210] Ashwin R             - Min Heap & Top-K Ranking\n");
    printf("  [25Z256] Santhosh Parthasarathi R - Hash Table & Frequency Management\n");
    printf("  [25Z271] Sujith E             - Integration, Optimization & Benchmarking\n");
    printf("  [25Z274] Tharun J             - React Frontend & Interface Testing\n");
    printf("----------------------------------------------------------------------\n");

    /* Determine port: check environment variable PORT (standard for Render: default 10000) */
    int port = 10000;
    const char *port_env = getenv("PORT");
    if (port_env && strlen(port_env) > 0) {
        port = atoi(port_env);
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        }
    }

    /* Determine database driver and target: support DATABASE_DRIVER, SQLITECLOUD_URL, and DATABASE_PATH */
    const char *driver_env = getenv("DATABASE_DRIVER");
    const char *db_path = "intellisuggest.db";
    const char *cloud_url = getenv("SQLITECLOUD_URL");

    if (driver_env && (strcmp(driver_env, "sqlitecloud") == 0 || strcmp(driver_env, "cloud") == 0)) {
        if (cloud_url && strlen(cloud_url) > 0) {
            db_path = cloud_url;
        } else {
            db_path = "sqlitecloud://";
        }
    } else {
        const char *db_env = getenv("DATABASE_PATH");
        if (db_env && strlen(db_env) > 0) {
            db_path = db_env;
        }
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        }
    }

    /* Determine sample dataset CSV path */
    const char *sample_csv = "database/sample_data.csv";
    const char *csv_env = getenv("SAMPLE_DATA_PATH");
    if (csv_env && strlen(csv_env) > 0) {
        sample_csv = csv_env;
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            sample_csv = argv[++i];
        }
    }

    /* Ensure target database directory exists for local files */
    if (strncmp(db_path, "sqlitecloud://", 14) != 0 && strncmp(db_path, "https://", 8) != 0) {
        ensure_parent_dir(db_path);
    }

    /* CLI test mode verification flag */
    if (argc > 1 && strcmp(argv[1], "--verify-only") == 0) {
        printf("[CLI Mode]: Initializing and verifying in-memory engine...\n");
        ServerContext ctx;
        if (!api_server_init(&ctx, port, db_path, sample_csv)) {
            fprintf(stderr, "Verification failed.\n");
            return EXIT_FAILURE;
        }
        printf("[CLI Mode]: System verified successfully. Active driver: %s\n", db_get_driver_name());
        api_server_cleanup(&ctx);
        return EXIT_SUCCESS;
    }

    ServerContext ctx;
    if (!api_server_init(&ctx, port, db_path, sample_csv)) {
        fprintf(stderr, "Fatal error: server initialization failed.\n");
        return EXIT_FAILURE;
    }

    api_server_start(&ctx);

    printf("[Server]: Listening on 0.0.0.0:%d [Driver: %s]\n", port, db_get_driver_name());
    printf("[Server]: Press Ctrl+C to terminate.\n");

    while (g_running) {
        sleep_sec(1);
    }

    api_server_stop();
    api_server_cleanup(&ctx);
    printf("[Server]: Terminated cleanly.\n");
    return EXIT_SUCCESS;
}
