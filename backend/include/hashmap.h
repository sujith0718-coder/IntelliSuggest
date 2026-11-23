#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdbool.h>
#include <stddef.h>

#define HASH_INITIAL_CAPACITY 64
#define HASH_MAX_LOAD_FACTOR 0.75f

/* Linked list node for separate chaining collision resolution */
typedef struct HashNode {
    char *keyword;
    int frequency;
    char created_at[32];
    char last_searched_at[32];
    bool is_active;
    struct HashNode *next;
} HashNode;

/* Custom Hash Table */
typedef struct {
    HashNode **buckets;
    size_t capacity;
    size_t size;
    size_t collision_count;
    float max_load_factor;
} HashTable;

/* Lifecycle */
HashTable* hashtable_create(size_t initial_capacity);
void hashtable_free(HashTable *ht);

/* Hash function (djb2 hash algorithm) */
unsigned long hashtable_hash(const char *str, size_t capacity);

/* Core Operations */
bool hashtable_insert(HashTable *ht, const char *keyword, int frequency, const char *created_at, const char *last_searched_at, bool is_active);
HashNode* hashtable_search(const HashTable *ht, const char *keyword);
bool hashtable_update_frequency(HashTable *ht, const char *keyword, int new_frequency, const char *last_searched_at);
bool hashtable_increment_frequency(HashTable *ht, const char *keyword, const char *searched_at, int *out_new_frequency);
bool hashtable_delete(HashTable *ht, const char *keyword);

/* Introspection & Metrics */
float hashtable_get_load_factor(const HashTable *ht);
size_t hashtable_get_collision_count(const HashTable *ht);
size_t hashtable_get_all_entries(const HashTable *ht, HashNode ***out_nodes);
void hashtable_free_entries_list(HashNode **nodes);

#endif /* HASHMAP_H */

