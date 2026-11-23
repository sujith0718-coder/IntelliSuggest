#include "api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int total_tests = 0;
static int passed_tests = 0;

#define TEST_ASSERT(cond, msg) do { \
    total_tests++; \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        passed_tests++; \
    } else { \
        printf("  [FAIL] %s (Line %d)\n", msg, __LINE__); \
    } \
} while(0)

/* Standalone test helper duplicating constant-time token comparison */
static bool test_verify_auth(const char *expected_key, const char *auth_hdr) {
    if (!expected_key || strlen(expected_key) == 0) return false; /* Fail-closed */
    if (!auth_hdr || strncmp(auth_hdr, "Bearer ", 7) != 0) return false;
    const char *provided_key = auth_hdr + 7;

    size_t len_a = strlen(expected_key);
    size_t len_b = strlen(provided_key);
    if (len_a != len_b) return false;

    unsigned char diff = 0;
    for (size_t i = 0; i < len_a; i++) {
        diff |= (unsigned char)(expected_key[i] ^ provided_key[i]);
    }
    return (diff == 0);
}

void test_admin_auth_logic(void) {
    printf("\n--- Test Suite 1: Administrative Authentication & Constant-Time Verification ---\n");

    const char *valid_secret = "super_secret_production_admin_key_98765";

    /* Unset / Empty Secret */
    TEST_ASSERT(!test_verify_auth(NULL, "Bearer some_key"), "Fail-closed when ADMIN_API_KEY is NULL");
    TEST_ASSERT(!test_verify_auth("", "Bearer some_key"), "Fail-closed when ADMIN_API_KEY is empty");

    /* Missing or Malformed Headers */
    TEST_ASSERT(!test_verify_auth(valid_secret, NULL), "Reject when Authorization header is NULL");
    TEST_ASSERT(!test_verify_auth(valid_secret, ""), "Reject when Authorization header is empty");
    TEST_ASSERT(!test_verify_auth(valid_secret, "Basic abc123xyz"), "Reject non-Bearer auth scheme");
    TEST_ASSERT(!test_verify_auth(valid_secret, "Bearer"), "Reject incomplete Bearer header");
    TEST_ASSERT(!test_verify_auth(valid_secret, "Bearer "), "Reject empty Bearer token");

    /* Invalid Tokens */
    TEST_ASSERT(!test_verify_auth(valid_secret, "Bearer wrong_token"), "Reject incorrect secret token");
    TEST_ASSERT(!test_verify_auth(valid_secret, "Bearer super_secret_production_admin_key_9876"), "Reject truncated token");
    TEST_ASSERT(!test_verify_auth(valid_secret, "Bearer super_secret_production_admin_key_98765_extra"), "Reject extended token");

    /* Valid Token */
    char valid_header[128];
    snprintf(valid_header, sizeof(valid_header), "Bearer %s", valid_secret);
    TEST_ASSERT(test_verify_auth(valid_secret, valid_header), "Accept exact Bearer secret token");
}

int main(void) {
    printf("====================================================\n");
    printf("     INTELLISUGGEST - ADMIN AUTH TEST SUITE         \n");
    printf("====================================================\n");

    test_admin_auth_logic();

    printf("\n====================================================\n");
    printf("Test Results: %d / %d passed (%.1f%%)\n",
           passed_tests, total_tests,
           (total_tests > 0 ? (float)passed_tests * 100.0f / total_tests : 0.0f));
    printf("====================================================\n");

    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}

