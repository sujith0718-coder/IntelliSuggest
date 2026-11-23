#ifndef API_H
#define API_H

#include "trie.h"
#include "hashmap.h"
#include "concept_index.h"
#include <stdbool.h>

/* Application context shared across HTTP handler threads */
typedef struct {
    Trie *trie;
    HashTable *hashmap;
    ConceptIndex *concepts;
    char db_path[256];
    int port;
} ServerContext;

/* Server lifecycle */
bool api_server_init(ServerContext *ctx, int port, const char *db_path, const char *sample_csv_path);
void api_server_start(ServerContext *ctx);
void api_server_stop(void);
void api_server_cleanup(ServerContext *ctx);

#endif /* API_H */

