#include "trie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Convert normalized character ('a'-'z' or ' ') to index 0-26 */
int trie_char_to_index(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a';
    }
    if (c == ' ') {
        return 26;
    }
    return -1;
}

/* Convert index 0-26 back to character */
char trie_index_to_char(int index) {
    if (index >= 0 && index < 26) {
        return (char)('a' + index);
    }
    if (index == 26) {
        return ' ';
    }
    return '\0';
}

/* Normalize string: lowercase, collapses spaces, strips unsupported chars */
char* trie_normalize(const char *input) {
    if (!input) return NULL;
    size_t len = strlen(input);
    char *cleaned = (char *)malloc(len + 1);
    if (!cleaned) return NULL;

    size_t out_idx = 0;
    bool prev_space = true; /* Prevents leading spaces */

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)input[i];
        if (isalpha(c)) {
            cleaned[out_idx++] = (char)tolower(c);
            prev_space = false;
        } else if (isspace(c) || c == '-' || c == '_') {
            if (!prev_space) {
                cleaned[out_idx++] = ' ';
                prev_space = true;
            }
        }
        /* other punctuation/symbols are omitted */
    }

    /* Remove trailing space if any */
    if (out_idx > 0 && cleaned[out_idx - 1] == ' ') {
        out_idx--;
    }
    cleaned[out_idx] = '\0';
    return cleaned;
}

/* Allocate a new TrieNode with zeroed memory */
static TrieNode* trie_node_create(void) {
    TrieNode *node = (TrieNode *)calloc(1, sizeof(TrieNode));
    if (!node) return NULL;
    node->is_end_of_word = false;
    node->keyword = NULL;
    node->frequency = 0;
    node->cache_count = 0;
    node->is_cache_valid = false;
    return node;
}

/* Create an empty Trie */
Trie* trie_create(void) {
    Trie *trie = (Trie *)malloc(sizeof(Trie));
    if (!trie) return NULL;

    trie->root = trie_node_create();
    if (!trie->root) {
        free(trie);
        return NULL;
    }
    trie->total_words = 0;
    trie->total_nodes = 1;
    return trie;
}

/* Recursive helper to free a TrieNode and its subtrees */
static void trie_node_free(TrieNode *node) {
    if (!node) return;
    for (int i = 0; i < TRIE_ALPHABET_SIZE; i++) {
        if (node->children[i]) {
            trie_node_free(node->children[i]);
            node->children[i] = NULL;
        }
    }
    if (node->keyword) {
        free(node->keyword);
        node->keyword = NULL;
    }
    free(node);
}

/* Free the entire Trie */
void trie_free(Trie *trie) {
    if (!trie) return;
    trie_node_free(trie->root);
    trie->root = NULL;
    free(trie);
}

/* Insert keyword with frequency into Trie */
bool trie_insert(Trie *trie, const char *keyword, int frequency) {
    if (!trie || !trie->root || !keyword) return false;

    char *norm = trie_normalize(keyword);
    if (!norm || strlen(norm) == 0) {
        free(norm);
        return false;
    }

    TrieNode *curr = trie->root;
    size_t len = strlen(norm);

    for (size_t i = 0; i < len; i++) {
        int idx = trie_char_to_index(norm[i]);
        if (idx < 0) continue;

        if (!curr->children[idx]) {
            curr->children[idx] = trie_node_create();
            if (!curr->children[idx]) {
                free(norm);
                return false;
            }
            trie->total_nodes++;
        }
        curr = curr->children[idx];
        /* Invalidate cache on ancestors during insertion */
        curr->is_cache_valid = false;
    }

    if (!curr->is_end_of_word) {
        curr->is_end_of_word = true;
        trie->total_words++;
    }

    if (curr->keyword) {
        free(curr->keyword);
    }
    curr->keyword = norm; /* Take ownership of normalized string */
    curr->frequency = frequency;
    return true;
}

/* Search exact keyword */
bool trie_search_exact(const Trie *trie, const char *keyword, int *out_frequency) {
    if (!trie || !trie->root || !keyword) return false;

    char *norm = trie_normalize(keyword);
    if (!norm) return false;

    TrieNode *curr = trie->root;
    size_t len = strlen(norm);

    for (size_t i = 0; i < len; i++) {
        int idx = trie_char_to_index(norm[i]);
        if (idx < 0 || !curr->children[idx]) {
            free(norm);
            return false;
        }
        curr = curr->children[idx];
    }

    free(norm);
    if (curr && curr->is_end_of_word) {
        if (out_frequency) {
            *out_frequency = curr->frequency;
        }
        return true;
    }
    return false;
}

/* Find node matching prefix */
TrieNode* trie_find_node(const Trie *trie, const char *prefix) {
    if (!trie || !trie->root || !prefix) return NULL;

    char *norm = trie_normalize(prefix);
    if (!norm) return NULL;

    TrieNode *curr = trie->root;
    size_t len = strlen(norm);

    for (size_t i = 0; i < len; i++) {
        int idx = trie_char_to_index(norm[i]);
        if (idx < 0 || !curr->children[idx]) {
            free(norm);
            return NULL;
        }
        curr = curr->children[idx];
    }

    free(norm);
    return curr;
}

/* Update frequency of existing keyword */
bool trie_update_frequency(Trie *trie, const char *keyword, int new_frequency) {
    if (!trie || !trie->root || !keyword) return false;

    char *norm = trie_normalize(keyword);
    if (!norm) return false;

    TrieNode *curr = trie->root;
    size_t len = strlen(norm);

    for (size_t i = 0; i < len; i++) {
        int idx = trie_char_to_index(norm[i]);
        if (idx < 0 || !curr->children[idx]) {
            free(norm);
            return false;
        }
        curr = curr->children[idx];
        curr->is_cache_valid = false; /* Invalidate cache along path */
    }

    free(norm);
    if (curr && curr->is_end_of_word) {
        curr->frequency = new_frequency;
        return true;
    }
    return false;
}

/* Helper to check if node has any child */
static bool trie_node_has_children(const TrieNode *node) {
    if (!node) return false;
    for (int i = 0; i < TRIE_ALPHABET_SIZE; i++) {
        if (node->children[i]) return true;
    }
    return false;
}

/* Recursive deletion helper */
static bool trie_delete_helper(TrieNode *curr, const char *norm, size_t depth, bool *deleted_word, size_t *deleted_nodes) {
    if (!curr) return false;

    if (depth == strlen(norm)) {
        if (!curr->is_end_of_word) {
            return false; /* Keyword does not exist */
        }
        curr->is_end_of_word = false;
        if (curr->keyword) {
            free(curr->keyword);
            curr->keyword = NULL;
        }
        curr->frequency = 0;
        curr->is_cache_valid = false;
        *deleted_word = true;

        /* If no children, signal that this node can be deleted */
        return !trie_node_has_children(curr);
    }

    int idx = trie_char_to_index(norm[depth]);
    if (idx < 0 || !curr->children[idx]) {
        return false;
    }

    bool should_delete_child = trie_delete_helper(curr->children[idx], norm, depth + 1, deleted_word, deleted_nodes);

    if (should_delete_child) {
        free(curr->children[idx]);
        curr->children[idx] = NULL;
        (*deleted_nodes)++;
    }

    curr->is_cache_valid = false;

    /* A node can be freed if it has no children AND is not an end of word */
    return (!curr->is_end_of_word && !trie_node_has_children(curr));
}

/* Delete keyword from Trie */
bool trie_delete(Trie *trie, const char *keyword) {
    if (!trie || !trie->root || !keyword) return false;

    char *norm = trie_normalize(keyword);
    if (!norm) return false;

    bool deleted_word = false;
    size_t deleted_nodes = 0;

    trie_delete_helper(trie->root, norm, 0, &deleted_word, &deleted_nodes);

    if (deleted_word) {
        trie->total_words--;
        trie->total_nodes -= deleted_nodes;
    }

    free(norm);
    return deleted_word;
}

/* Depth-first collection of all words below a node */
static void trie_dfs_collect(const TrieNode *node, KeywordMatch **matches, size_t *count, size_t *capacity) {
    if (!node) return;

    if (node->is_end_of_word && node->keyword) {
        if (*count >= *capacity) {
            size_t new_cap = (*capacity == 0) ? 16 : (*capacity * 2);
            KeywordMatch *new_matches = (KeywordMatch *)realloc(*matches, new_cap * sizeof(KeywordMatch));
            if (!new_matches) return;
            *matches = new_matches;
            *capacity = new_cap;
        }

        (*matches)[*count].keyword = strdup(node->keyword);
        (*matches)[*count].frequency = node->frequency;
        (*count)++;
    }

    for (int i = 0; i < TRIE_ALPHABET_SIZE; i++) {
        if (node->children[i]) {
            trie_dfs_collect(node->children[i], matches, count, capacity);
        }
    }
}

/* Collect all matches for a prefix */
size_t trie_collect_matches(const Trie *trie, const char *prefix, KeywordMatch **out_matches) {
    if (!trie || !out_matches) return 0;

    *out_matches = NULL;
    size_t count = 0;
    size_t capacity = 0;

    TrieNode *node = NULL;
    if (!prefix || strlen(prefix) == 0) {
        node = trie->root;
    } else {
        node = trie_find_node(trie, prefix);
    }

    if (!node) return 0;

    trie_dfs_collect(node, out_matches, &count, &capacity);
    return count;
}

/* Free collected match array */
void trie_free_matches(KeywordMatch *matches, size_t count) {
    if (!matches) return;
    for (size_t i = 0; i < count; i++) {
        if (matches[i].keyword) {
            free(matches[i].keyword);
        }
    }
    free(matches);
}

