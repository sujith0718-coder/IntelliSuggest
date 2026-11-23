#include "hashmap.h"
#include "trie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Implementation of the classic djb2 hash algorithm by Dan Bernstein */
unsigned long hashtable_hash(const char *str, size_t capacity) {
    if (!str || capacity == 0) return 0;
    unsigned long hash = 5381;
    int c;

    while ((c = (unsigned char)*str++)) {
        /* hash * 33 + c */
        hash = ((hash << 5) + hash) + (unsigned long)c;
    }

    return hash % capacity;
}

/* Get current UTC ISO timestamp */
static void get_current_timestamp(char *buffer, size_t max_len) {
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    if (tm_info) {
        strftime(buffer, max_len, "%Y-%m-%dT%H:%M:%SZ", tm_info);
    } else {
        snprintf(buffer, max_len, "1970-01-01T00:00:00Z");
    }
}

/* Allocate and initialize a new Hash Table */
HashTable* hashtable_create(size_t initial_capacity) {
    size_t cap = (initial_capacity < HASH_INITIAL_CAPACITY) ? HASH_INITIAL_CAPACITY : initial_capacity;

    HashTable *ht = (HashTable *)malloc(sizeof(HashTable));
    if (!ht) return NULL;

    ht->buckets = (HashNode **)calloc(cap, sizeof(HashNode *));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }

    ht->capacity = cap;
    ht->size = 0;
    ht->collision_count = 0;
    ht->max_load_factor = HASH_MAX_LOAD_FACTOR;
    return ht;
}

/* Free all nodes and memory used by the Hash Table */
void hashtable_free(HashTable *ht) {
    if (!ht) return;

    for (size_t i = 0; i < ht->capacity; i++) {
        HashNode *curr = ht->buckets[i];
        while (curr) {
            HashNode *next = curr->next;
            if (curr->keyword) free(curr->keyword);
            free(curr);
            curr = next;
        }
    }

    free(ht->buckets);
    free(ht);
}

/* Dynamic rehashing when load factor threshold is crossed */
static bool hashtable_rehash(HashTable *ht) {
    size_t new_cap = ht->capacity * 2;
    HashNode **new_buckets = (HashNode **)calloc(new_cap, sizeof(HashNode *));
    if (!new_buckets) return false;

    size_t new_collisions = 0;

    for (size_t i = 0; i < ht->capacity; i++) {
        HashNode *curr = ht->buckets[i];
        while (curr) {
            HashNode *next = curr->next;
            unsigned long new_idx = hashtable_hash(curr->keyword, new_cap);

            if (new_buckets[new_idx] != NULL) {
                new_collisions++;
            }

            curr->next = new_buckets[new_idx];
            new_buckets[new_idx] = curr;
            curr = next;
        }
    }

    free(ht->buckets);
    ht->buckets = new_buckets;
    ht->capacity = new_cap;
    ht->collision_count = new_collisions;
    return true;
}

/* Insert or update a keyword in the Hash Table */
bool hashtable_insert(HashTable *ht, const char *keyword, int frequency, const char *created_at, const char *last_searched_at, bool is_active) {
    if (!ht || !keyword) return false;

    char *norm = trie_normalize(keyword);
    if (!norm || strlen(norm) == 0) {
        free(norm);
        return false;
    }

    /* Check if load factor exceeds threshold */
    if ((float)(ht->size + 1) / (float)ht->capacity > ht->max_load_factor) {
        hashtable_rehash(ht);
    }

    unsigned long idx = hashtable_hash(norm, ht->capacity);
    HashNode *curr = ht->buckets[idx];

    /* Search bucket for existing key */
    while (curr) {
        if (strcmp(curr->keyword, norm) == 0) {
            /* Update existing entry */
            curr->frequency = frequency;
            curr->is_active = is_active;
            if (last_searched_at) {
                strncpy(curr->last_searched_at, last_searched_at, sizeof(curr->last_searched_at) - 1);
                curr->last_searched_at[sizeof(curr->last_searched_at) - 1] = '\0';
            }
            free(norm);
            return true;
        }
        curr = curr->next;
    }

    /* Create new node */
    HashNode *new_node = (HashNode *)malloc(sizeof(HashNode));
    if (!new_node) {
        free(norm);
        return false;
    }

    new_node->keyword = norm;
    new_node->frequency = frequency;
    new_node->is_active = is_active;

    if (created_at && strlen(created_at) > 0) {
        strncpy(new_node->created_at, created_at, sizeof(new_node->created_at) - 1);
    } else {
        get_current_timestamp(new_node->created_at, sizeof(new_node->created_at));
    }
    new_node->created_at[sizeof(new_node->created_at) - 1] = '\0';

    if (last_searched_at && strlen(last_searched_at) > 0) {
        strncpy(new_node->last_searched_at, last_searched_at, sizeof(new_node->last_searched_at) - 1);
    } else {
        new_node->last_searched_at[0] = '\0';
    }
    new_node->last_searched_at[sizeof(new_node->last_searched_at) - 1] = '\0';

    if (ht->buckets[idx] != NULL) {
        ht->collision_count++;
    }

    /* Prepend to separate chain */
    new_node->next = ht->buckets[idx];
    ht->buckets[idx] = new_node;
    ht->size++;

    return true;
}

/* Lookup keyword in Hash Table */
HashNode* hashtable_search(const HashTable *ht, const char *keyword) {
    if (!ht || !keyword) return NULL;

    char *norm = trie_normalize(keyword);
    if (!norm) return NULL;

    unsigned long idx = hashtable_hash(norm, ht->capacity);
    HashNode *curr = ht->buckets[idx];

    while (curr) {
        if (strcmp(curr->keyword, norm) == 0) {
            free(norm);
            return curr;
        }
        curr = curr->next;
    }

    free(norm);
    return NULL;
}

/* Update frequency of existing keyword */
bool hashtable_update_frequency(HashTable *ht, const char *keyword, int new_frequency, const char *last_searched_at) {
    HashNode *node = hashtable_search(ht, keyword);
    if (!node) return false;

    node->frequency = new_frequency;
    if (last_searched_at) {
        strncpy(node->last_searched_at, last_searched_at, sizeof(node->last_searched_at) - 1);
        node->last_searched_at[sizeof(node->last_searched_at) - 1] = '\0';
    } else {
        get_current_timestamp(node->last_searched_at, sizeof(node->last_searched_at));
    }
    return true;
}

/* Increment frequency when a search is submitted */
bool hashtable_increment_frequency(HashTable *ht, const char *keyword, const char *searched_at, int *out_new_frequency) {
    HashNode *node = hashtable_search(ht, keyword);
    if (!node) {
        /* If keyword does not exist, insert it with initial frequency 1 */
        char now[32];
        if (searched_at && strlen(searched_at) > 0) {
            strncpy(now, searched_at, sizeof(now) - 1);
            now[sizeof(now) - 1] = '\0';
        } else {
            get_current_timestamp(now, sizeof(now));
        }

        if (!hashtable_insert(ht, keyword, 1, now, now, true)) {
            return false;
        }
        if (out_new_frequency) *out_new_frequency = 1;
        return true;
    }

    node->frequency++;
    if (searched_at && strlen(searched_at) > 0) {
        strncpy(node->last_searched_at, searched_at, sizeof(node->last_searched_at) - 1);
        node->last_searched_at[sizeof(node->last_searched_at) - 1] = '\0';
    } else {
        get_current_timestamp(node->last_searched_at, sizeof(node->last_searched_at));
    }

    if (out_new_frequency) {
        *out_new_frequency = node->frequency;
    }
    return true;
}

/* Delete keyword from Hash Table */
bool hashtable_delete(HashTable *ht, const char *keyword) {
    if (!ht || !keyword) return false;

    char *norm = trie_normalize(keyword);
    if (!norm) return false;

    unsigned long idx = hashtable_hash(norm, ht->capacity);
    HashNode *curr = ht->buckets[idx];
    HashNode *prev = NULL;

    while (curr) {
        if (strcmp(curr->keyword, norm) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                ht->buckets[idx] = curr->next;
            }

            if (curr->keyword) free(curr->keyword);
            free(curr);
            ht->size--;
            free(norm);
            return true;
        }
        prev = curr;
        curr = curr->next;
    }

    free(norm);
    return false;
}

/* Calculate current load factor */
float hashtable_get_load_factor(const HashTable *ht) {
    if (!ht || ht->capacity == 0) return 0.0f;
    return (float)ht->size / (float)ht->capacity;
}

/* Get current collision count */
size_t hashtable_get_collision_count(const HashTable *ht) {
    if (!ht) return 0;
    return ht->collision_count;
}

/* Collect flat array of all node pointers */
size_t hashtable_get_all_entries(const HashTable *ht, HashNode ***out_nodes) {
    if (!ht || !out_nodes) return 0;

    HashNode **nodes = (HashNode **)malloc(ht->size * sizeof(HashNode *));
    if (!nodes && ht->size > 0) return 0;

    size_t count = 0;
    for (size_t i = 0; i < ht->capacity; i++) {
        HashNode *curr = ht->buckets[i];
        while (curr) {
            nodes[count++] = curr;
            curr = curr->next;
        }
    }

    *out_nodes = nodes;
    return count;
}

/* Free collected node pointer array (does NOT free actual nodes) */
void hashtable_free_entries_list(HashNode **nodes) {
    if (nodes) {
        free(nodes);
    }
}

