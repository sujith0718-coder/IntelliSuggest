/**
 * ============================================================================
 * PROJECT: INTELLISUGGEST
 * A High-Performance, Frequency-Ranked Autocomplete and Search Suggestion Engine
 * ----------------------------------------------------------------------------
 * Academic Data Structures and Algorithms Project - Team 12
 * ============================================================================
 */

#include "api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#if defined(_WIN32)
#include <windows.h>
#define sleep_sec(s) Sleep((s) * 1000)
#else
#include <unistd.h>
#define sleep_sec(s) sleep(s)
#endif

static volatile int g_running = 1;

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

    /* Determine port: check environment variable PORT (standard for Render / cloud) */
    int port = 8080;
    const char *port_env = getenv("PORT");
    if (port_env && strlen(port_env) > 0) {
        port = atoi(port_env);
    }
    if (argc > 1 && strcmp(argv[1], "--port") == 0 && argc > 2) {
        port = atoi(argv[2]);
    }

    /* CLI test mode verification flag */
    if (argc > 1 && strcmp(argv[1], "--verify-only") == 0) {
        printf("[CLI Mode]: Initializing and verifying in-memory engine...\n");
        ServerContext ctx;
        if (!api_server_init(&ctx, port, "intellisuggest.db", "database/sample_data.csv")) {
            fprintf(stderr, "Verification failed.\n");
            return EXIT_FAILURE;
        }
        printf("[CLI Mode]: System verified successfully.\n");
        api_server_cleanup(&ctx);
        return EXIT_SUCCESS;
    }

    ServerContext ctx;
    if (!api_server_init(&ctx, port, "intellisuggest.db", "database/sample_data.csv")) {
        fprintf(stderr, "Fatal error: server initialization failed.\n");
        return EXIT_FAILURE;
    }

    api_server_start(&ctx);

    printf("[Server]: Listening on http://localhost:%d\n", port);
    printf("[Server]: Press Ctrl+C to terminate.\n");

    while (g_running) {
        sleep_sec(1);
    }

    api_server_cleanup(&ctx);
    printf("[Server]: Terminated cleanly.\n");
    return EXIT_SUCCESS;
}
