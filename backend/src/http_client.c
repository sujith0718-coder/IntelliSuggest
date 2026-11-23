#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>

bool http_post_json(const char *url, const char *auth_bearer, const char *json_body, HttpResponse *resp, int timeout_ms) {
    if (!resp) return false;
    memset(resp, 0, sizeof(*resp));
    resp->status_code = 0;

    if (!url || !json_body) {
        snprintf(resp->error_msg, sizeof(resp->error_msg), "Invalid URL or JSON payload");
        return false;
    }

    /* Parse URL into host and path (expecting https://...) */
    const char *prefix = "https://";
    if (strncmp(url, prefix, strlen(prefix)) != 0) {
        snprintf(resp->error_msg, sizeof(resp->error_msg), "Only HTTPS URLs are supported");
        return false;
    }

    const char *host_start = url + strlen(prefix);
    const char *slash = strchr(host_start, '/');
    char hostname[256] = {0};
    char path[512] = "/";

    if (slash) {
        size_t host_len = slash - host_start;
        if (host_len >= sizeof(hostname)) host_len = sizeof(hostname) - 1;
        strncpy(hostname, host_start, host_len);
        strncpy(path, slash, sizeof(path) - 1);
    } else {
        strncpy(hostname, host_start, sizeof(hostname) - 1);
    }

    /* Convert to wide strings for WinHTTP */
    WCHAR wHost[256];
    WCHAR wPath[512];
    MultiByteToWideChar(CP_UTF8, 0, hostname, -1, wHost, 256);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, 512);

    HINTERNET hSession = WinHttpOpen(L"IntelliSuggest-Client/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        snprintf(resp->error_msg, sizeof(resp->error_msg), "WinHttpOpen failed (err %lu)", GetLastError());
        return false;
    }

    if (timeout_ms <= 0) timeout_ms = 10000;
    WinHttpSetTimeouts(hSession, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

    HINTERNET hConnect = WinHttpConnect(hSession, wHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        snprintf(resp->error_msg, sizeof(resp->error_msg), "WinHttpConnect failed (err %lu)", GetLastError());
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wPath, NULL,
                                            NULL,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        snprintf(resp->error_msg, sizeof(resp->error_msg), "WinHttpOpenRequest failed (err %lu)", GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    /* Build headers */
    char headers_buf[1024];
    if (auth_bearer && strlen(auth_bearer) > 0) {
        snprintf(headers_buf, sizeof(headers_buf),
                 "Content-Type: application/json\r\nAuthorization: Bearer %s\r\n", auth_bearer);
    } else {
        snprintf(headers_buf, sizeof(headers_buf), "Content-Type: application/json\r\n");
    }

    WCHAR wHeaders[1024];
    MultiByteToWideChar(CP_UTF8, 0, headers_buf, -1, wHeaders, 1024);

    DWORD body_len = (DWORD)strlen(json_body);
    BOOL bResults = WinHttpSendRequest(hRequest,
                                       wHeaders, -1L,
                                       (LPVOID)json_body, body_len,
                                       body_len, 0);

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (!bResults) {
        snprintf(resp->error_msg, sizeof(resp->error_msg), "WinHTTP request failed (err %lu)", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    /* Retrieve status code */
    DWORD statusCode = 0;
    DWORD dwSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &statusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
    resp->status_code = (int)statusCode;

    /* Read response stream */
    size_t alloc_size = 4096;
    resp->body = (char*)malloc(alloc_size);
    if (!resp->body) {
        snprintf(resp->error_msg, sizeof(resp->error_msg), "Memory allocation failure");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    resp->body[0] = '\0';
    resp->body_len = 0;

    DWORD dwDownloaded = 0;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;

        if (resp->body_len + dwSize + 1 > alloc_size) {
            alloc_size = (resp->body_len + dwSize + 1) * 2;
            char *new_buf = (char*)realloc(resp->body, alloc_size);
            if (!new_buf) break;
            resp->body = new_buf;
        }

        if (WinHttpReadData(hRequest, resp->body + resp->body_len, dwSize, &dwDownloaded)) {
            resp->body_len += dwDownloaded;
            resp->body[resp->body_len] = '\0';
        }
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return (resp->status_code >= 200 && resp->status_code < 300);
}

#else
/* Linux / POSIX using libcurl */
#include <curl/curl.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

bool http_post_json(const char *url, const char *auth_bearer, const char *json_body, HttpResponse *resp, int timeout_ms) {
    if (!resp) return false;
    memset(resp, 0, sizeof(*resp));
    resp->status_code = 0;

    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(resp->error_msg, sizeof(resp->error_msg), "curl_easy_init failed");
        return false;
    }

    struct MemoryStruct chunk;
    chunk.memory = (char*)malloc(1);
    chunk.size = 0;
    if (chunk.memory) chunk.memory[0] = '\0';

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (auth_bearer && strlen(auth_bearer) > 0) {
        char auth_hdr[512];
        snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", auth_bearer);
        headers = curl_slist_append(headers, auth_hdr);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)(timeout_ms > 0 ? timeout_ms : 10000));
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "IntelliSuggest-Client/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        snprintf(resp->error_msg, sizeof(resp->error_msg), "curl_easy_perform failed: %s", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        if (chunk.memory) free(chunk.memory);
        return false;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    resp->status_code = (int)http_code;
    resp->body = chunk.memory;
    resp->body_len = chunk.size;

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return (resp->status_code >= 200 && resp->status_code < 300);
}
#endif

void http_response_free(HttpResponse *resp) {
    if (resp && resp->body) {
        free(resp->body);
        resp->body = NULL;
        resp->body_len = 0;
    }
}
