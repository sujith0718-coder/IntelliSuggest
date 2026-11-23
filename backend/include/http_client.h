#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int status_code;
    char *body;
    size_t body_len;
    char error_msg[256];
} HttpResponse;

/**
 * Perform an HTTPS POST request sending a JSON body.
 *
 * @param url Full HTTPS URL (e.g. "https://coko3dsroz.g5.gateway.sqlite.cloud/v2/weblite/sql")
 * @param auth_bearer Optional Bearer token (API key)
 * @param json_body JSON payload string
 * @param resp Output response struct (caller must free with http_response_free)
 * @param timeout_ms Request timeout in milliseconds (e.g. 5000)
 * @return true if request succeeded (HTTP 2xx), false otherwise
 */
bool http_post_json(const char *url, const char *auth_bearer, const char *json_body, HttpResponse *resp, int timeout_ms);

/**
 * Free memory associated with HttpResponse.
 */
void http_response_free(HttpResponse *resp);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_CLIENT_H */

