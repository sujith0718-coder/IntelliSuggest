#ifndef TRIE_H
#define TRIE_H

#include <stdbool.h>
#include <stddef.h>

#define TRIE_ALPHABET_SIZE 27
#define TOP_K_SIZE 10

/* Cached suggestion representation used inside Trie nodes */
typedef struct {
    char keyword[128];
    int frequency;
} CachedSuggestion;

/* Node in the Trie */
typedef struct TrieNode {
    struct TrieNode *children[TRIE_ALPHABET_SIZE];
    bool is_end_of_word;
    char *keyword;              /* Full keyword stored at terminal node */
    int frequency;              /* Stored popularity */

    /* Top-10 Cache (Approach B) */
    CachedSuggestion top10_cache[TOP_K_SIZE];
    size_t cache_count;
    bool is_cache_valid;
} TrieNode;

/* Root structure managing the Trie */
typedef struct {
    TrieNode *root;
    size_t total_words;
    size_t total_nodes;
} Trie;

/* Keyword match item used for prefix search collections */
typedef struct {
    char *keyword;
    int frequency;
} KeywordMatch;

/* Lifecycle and Utility */
Trie* trie_create(void);
void trie_free(Trie *trie);
int trie_char_to_index(char c);
char trie_index_to_char(int index);
char* trie_normalize(const char *input);

/* Core Operations */
bool trie_insert(Trie *trie, const char *keyword, int frequency);
bool trie_search_exact(const Trie *trie, const char *keyword, int *out_frequency);
TrieNode* trie_find_node(const Trie *trie, const char *prefix);
bool trie_update_frequency(Trie *trie, const char *keyword, int new_frequency);
bool trie_delete(Trie *trie, const char *keyword);

/* Subtree Collection (Uncached Approach A) */
size_t trie_collect_matches(const Trie *trie, const char *prefix, KeywordMatch **out_matches);
void trie_free_matches(KeywordMatch *matches, size_t count);

#endif /* TRIE_H */

