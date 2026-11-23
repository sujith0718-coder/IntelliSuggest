#include "concept_index.h"
#include "trie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* Standard list of English stop-words to eliminate uninformative tokens */
static const char *STOP_WORDS[] = {
    "a", "about", "above", "after", "again", "against", "all", "am", "an", "and", "any", "are", "aren't",
    "as", "at", "be", "because", "been", "before", "being", "below", "between", "both", "but", "by", "can",
    "can't", "cannot", "could", "couldn't", "did", "didn't", "do", "does", "doesn't", "doing", "don't",
    "down", "during", "each", "few", "for", "from", "further", "had", "hadn't", "has", "hasn't", "have",
    "haven't", "having", "he", "he'd", "he'll", "he's", "her", "here", "here's", "hers", "herself", "him",
    "himself", "his", "how", "how's", "i", "i'd", "i'll", "i'm", "i've", "if", "in", "into", "is", "isn't",
    "it", "it's", "its", "itself", "let's", "me", "more", "most", "mustn't", "my", "myself", "no", "nor",
    "not", "of", "off", "on", "once", "only", "or", "other", "ought", "our", "ours", "ourselves", "out",
    "over", "own", "same", "shan't", "she", "she'd", "she'll", "she's", "should", "shouldn't", "so", "some",
    "such", "than", "that", "that's", "the", "their", "theirs", "them", "themselves", "then", "there",
    "there's", "these", "they", "they'd", "they'll", "they're", "they've", "this", "those", "through", "to",
    "too", "under", "until", "up", "very", "was", "wasn't", "we", "we'd", "we'll", "we're", "we've", "were",
    "weren't", "what", "what's", "when", "when's", "where", "where's", "which", "while", "who", "who's",
    "whom", "why", "why's", "with", "won't", "would", "wouldn't", "you", "you'd", "you'll", "you're", "you've",
    "your", "yours", "yourself", "yourselves",
    /* Common search filler phrases */
    "something", "used", "thing", "device", "technique", "method", "way", "tool", NULL
};

bool is_stop_word(const char *word) {
    if (!word) return true;
    for (int i = 0; STOP_WORDS[i] != NULL; i++) {
        if (strcmp(word, STOP_WORDS[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* Tokenize and filter stop-words */
size_t tokenize_and_filter(const char *text, char tokens[MAX_TOKENS_PER_QUERY][64]) {
    if (!text) return 0;

    size_t count = 0;
    char buffer[1024];
    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    /* Convert to lowercase and replace non-alphanumeric with spaces */
    for (size_t i = 0; buffer[i]; i++) {
        unsigned char c = (unsigned char)buffer[i];
        if (isalnum(c)) {
            buffer[i] = (char)tolower(c);
        } else {
            buffer[i] = ' ';
        }
    }

    char *saveptr = NULL;
    char *token = strtok_r(buffer, " ", &saveptr);
    while (token && count < MAX_TOKENS_PER_QUERY) {
        if (strlen(token) > 1 && !is_stop_word(token)) {
            /* Check duplicate in current query */
            bool dup = false;
            for (size_t j = 0; j < count; j++) {
                if (strcmp(tokens[j], token) == 0) {
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                strncpy(tokens[count], token, 63);
                tokens[count][63] = '\0';
                count++;
            }
        }
        token = strtok_r(NULL, " ", &saveptr);
    }

    return count;
}

ConceptIndex* concept_index_create(void) {
    ConceptIndex *idx = (ConceptIndex *)calloc(1, sizeof(ConceptIndex));
    if (!idx) return NULL;

    idx->index_capacity = 256;
    idx->index_entries = (InvertedIndexEntry *)calloc(idx->index_capacity, sizeof(InvertedIndexEntry));
    idx->index_size = 0;
    idx->concept_count = 0;
    return idx;
}

void concept_index_clear(ConceptIndex *idx) {
    if (!idx) return;

    for (size_t i = 0; i < idx->index_size; i++) {
        PostingNode *curr = idx->index_entries[i].postings;
        while (curr) {
            PostingNode *next = curr->next;
            free(curr);
            curr = next;
        }
        idx->index_entries[i].postings = NULL;
        idx->index_entries[i].token[0] = '\0';
    }
    idx->index_size = 0;
    idx->concept_count = 0;
}

/* Forward declaration */
static void add_posting(ConceptIndex *idx, const char *token, int concept_id, double weight);

/* Helper to rebuild postings from current concepts array */
static void rebuild_postings_from_concepts(ConceptIndex *idx) {
    if (!idx) return;

    /* Free existing postings */
    for (size_t i = 0; i < idx->index_size; i++) {
        PostingNode *curr = idx->index_entries[i].postings;
        while (curr) {
            PostingNode *next = curr->next;
            free(curr);
            curr = next;
        }
        idx->index_entries[i].postings = NULL;
    }
    idx->index_size = 0;

    /* Re-index all concepts */
    for (size_t c = 0; c < idx->concept_count; c++) {
        ConceptRecord *cr = &idx->concepts[c];

        /* Name tokens */
        char name_tokens[MAX_TOKENS_PER_QUERY][64];
        size_t n_tokens = tokenize_and_filter(cr->concept_name, name_tokens);
        for (size_t i = 0; i < n_tokens; i++) {
            add_posting(idx, name_tokens[i], cr->id, 4.0);
        }

        /* Keywords tokens */
        for (size_t k = 0; k < cr->keyword_count; k++) {
            char kw_tokens[MAX_TOKENS_PER_QUERY][64];
            size_t kw_tok_cnt = tokenize_and_filter(cr->related_keywords[k], kw_tokens);
            for (size_t i = 0; i < kw_tok_cnt; i++) {
                add_posting(idx, kw_tokens[i], cr->id, 3.0);
            }
        }

        /* Description tokens */
        char desc_tokens[MAX_TOKENS_PER_QUERY][64];
        size_t d_tokens = tokenize_and_filter(cr->description, desc_tokens);
        for (size_t i = 0; i < d_tokens; i++) {
            add_posting(idx, desc_tokens[i], cr->id, 1.5);
        }
    }
}

bool concept_index_update(ConceptIndex *idx, int id, const char *name, const char *description, const char *keywords_csv) {
    if (!idx || !name || !description) return false;

    /* Find existing concept */
    ConceptRecord *target = NULL;
    for (size_t i = 0; i < idx->concept_count; i++) {
        if (idx->concepts[i].id == id) {
            target = &idx->concepts[i];
            break;
        }
    }

    if (!target) {
        return concept_index_add(idx, id, name, description, keywords_csv);
    }

    strncpy(target->concept_name, name, sizeof(target->concept_name) - 1);
    target->concept_name[sizeof(target->concept_name) - 1] = '\0';

    strncpy(target->description, description, sizeof(target->description) - 1);
    target->description[sizeof(target->description) - 1] = '\0';

    target->keyword_count = 0;
    if (keywords_csv && strlen(keywords_csv) > 0) {
        char kw_buf[512];
        strncpy(kw_buf, keywords_csv, sizeof(kw_buf) - 1);
        kw_buf[sizeof(kw_buf) - 1] = '\0';

        char *saveptr = NULL;
        char *item = strtok_r(kw_buf, ",;", &saveptr);
        while (item && target->keyword_count < MAX_CONCEPT_KEYWORDS) {
            while (*item == ' ') item++;
            char *end = item + strlen(item) - 1;
            while (end > item && *end == ' ') { *end = '\0'; end--; }

            if (strlen(item) > 0) {
                strncpy(target->related_keywords[target->keyword_count], item, 127);
                target->related_keywords[target->keyword_count][127] = '\0';
                target->keyword_count++;
            }
            item = strtok_r(NULL, ",;", &saveptr);
        }
    }

    rebuild_postings_from_concepts(idx);
    return true;
}

bool concept_index_delete(ConceptIndex *idx, int id) {
    if (!idx || idx->concept_count == 0) return false;

    size_t found_idx = (size_t)-1;
    for (size_t i = 0; i < idx->concept_count; i++) {
        if (idx->concepts[i].id == id) {
            found_idx = i;
            break;
        }
    }

    if (found_idx == (size_t)-1) return false;

    /* Shift remaining concepts */
    for (size_t i = found_idx; i + 1 < idx->concept_count; i++) {
        idx->concepts[i] = idx->concepts[i + 1];
    }
    idx->concept_count--;

    rebuild_postings_from_concepts(idx);
    return true;
}

void concept_index_free(ConceptIndex *idx) {
    if (!idx) return;

    for (size_t i = 0; i < idx->index_size; i++) {
        PostingNode *curr = idx->index_entries[i].postings;
        while (curr) {
            PostingNode *next = curr->next;
            free(curr);
            curr = next;
        }
    }
    free(idx->index_entries);
    free(idx);
}

/* Internal posting adder */
static void add_posting(ConceptIndex *idx, const char *token, int concept_id, double weight) {
    if (!idx || !token || strlen(token) == 0) return;

    /* Check if token entry exists */
    InvertedIndexEntry *entry = NULL;
    for (size_t i = 0; i < idx->index_size; i++) {
        if (strcmp(idx->index_entries[i].token, token) == 0) {
            entry = &idx->index_entries[i];
            break;
        }
    }

    /* Not found: create new token entry */
    if (!entry) {
        if (idx->index_size >= idx->index_capacity) {
            size_t new_cap = idx->index_capacity * 2;
            InvertedIndexEntry *new_entries = (InvertedIndexEntry *)realloc(idx->index_entries, new_cap * sizeof(InvertedIndexEntry));
            if (!new_entries) return;
            idx->index_entries = new_entries;
            idx->index_capacity = new_cap;
        }

        entry = &idx->index_entries[idx->index_size++];
        strncpy(entry->token, token, 63);
        entry->token[63] = '\0';
        entry->postings = NULL;
    }

    /* Check if concept_id already in postings; if so, accumulate weight */
    PostingNode *curr = entry->postings;
    while (curr) {
        if (curr->concept_id == concept_id) {
            curr->weight += weight;
            return;
        }
        curr = curr->next;
    }

    /* Prepend new posting */
    PostingNode *p = (PostingNode *)malloc(sizeof(PostingNode));
    if (!p) return;
    p->concept_id = concept_id;
    p->weight = weight;
    p->next = entry->postings;
    entry->postings = p;
}

/* Index a concept's name, description, and keywords */
bool concept_index_add(ConceptIndex *idx, int id, const char *name, const char *description, const char *keywords_csv) {
    if (!idx || !name || !description || idx->concept_count >= MAX_CONCEPTS) return false;

    size_t c_idx = idx->concept_count++;
    ConceptRecord *c = &idx->concepts[c_idx];
    c->id = (id > 0) ? id : (int)c_idx + 1;

    strncpy(c->concept_name, name, sizeof(c->concept_name) - 1);
    c->concept_name[sizeof(c->concept_name) - 1] = '\0';

    strncpy(c->description, description, sizeof(c->description) - 1);
    c->description[sizeof(c->description) - 1] = '\0';

    c->keyword_count = 0;
    if (keywords_csv && strlen(keywords_csv) > 0) {
        char kw_buf[512];
        strncpy(kw_buf, keywords_csv, sizeof(kw_buf) - 1);
        kw_buf[sizeof(kw_buf) - 1] = '\0';

        char *saveptr = NULL;
        char *item = strtok_r(kw_buf, ",;", &saveptr);
        while (item && c->keyword_count < MAX_CONCEPT_KEYWORDS) {
            /* Trim spaces */
            while (*item == ' ') item++;
            char *end = item + strlen(item) - 1;
            while (end > item && *end == ' ') { *end = '\0'; end--; }

            if (strlen(item) > 0) {
                strncpy(c->related_keywords[c->keyword_count], item, 127);
                c->related_keywords[c->keyword_count][127] = '\0';
                c->keyword_count++;
            }
            item = strtok_r(NULL, ",;", &saveptr);
        }
    }

    /* Index Concept Name tokens (Weight 4.0) */
    char name_tokens[MAX_TOKENS_PER_QUERY][64];
    size_t n_tokens = tokenize_and_filter(name, name_tokens);
    for (size_t i = 0; i < n_tokens; i++) {
        add_posting(idx, name_tokens[i], c->id, 4.0);
    }

    /* Index Related Keywords tokens (Weight 3.0) */
    for (size_t k = 0; k < c->keyword_count; k++) {
        char kw_tokens[MAX_TOKENS_PER_QUERY][64];
        size_t kw_tok_cnt = tokenize_and_filter(c->related_keywords[k], kw_tokens);
        for (size_t i = 0; i < kw_tok_cnt; i++) {
            add_posting(idx, kw_tokens[i], c->id, 3.0);
        }
    }

    /* Index Description tokens (Weight 1.5) */
    char desc_tokens[MAX_TOKENS_PER_QUERY][64];
    size_t d_tokens = tokenize_and_filter(description, desc_tokens);
    for (size_t i = 0; i < d_tokens; i++) {
        add_posting(idx, desc_tokens[i], c->id, 1.5);
    }

    return true;
}

/* Seed high-value default concepts */
void concept_index_seed_defaults(ConceptIndex *idx) {
    concept_index_add(idx, 1, "Memoization",
        "A technique that stores previously computed results to avoid repeated calculations and optimize recursion",
        "caching, dynamic programming, stored results, lookup table, cache");

    concept_index_add(idx, 2, "Dijkstra Algorithm",
        "Algorithm that finds the shortest path between nodes in a weighted graph with non-negative edge weights",
        "shortest path, graph search, priority queue, single source shortest path");

    concept_index_add(idx, 3, "HDMI Adapter",
        "Device or cable used to connect a laptop or computer to a projector, monitor or external display",
        "usb-c to hdmi converter, display adapter, projector connector, displayport to hdmi");

    concept_index_add(idx, 4, "Laptop Cooling Pad",
        "External accessory used to cool a laptop and prevent thermal throttling or overheating with fans",
        "cooling stand, laptop cooler, fan pad, thermal mat");

    concept_index_add(idx, 5, "Binary Search",
        "Search algorithm that finds the position of a target value within a sorted array by repeatedly dividing the search range in half",
        "divide and conquer, logarithmic search, sorted array lookup");

    concept_index_add(idx, 6, "LRU Cache",
        "A caching data structure that evicts the least recently used elements first when capacity limit is reached",
        "least recently used, cache eviction policy, doubly linked list hashmap");

    concept_index_add(idx, 7, "Depth First Search",
        "Graph traversal algorithm that explores as deep as possible along each branch before backtracking",
        "dfs, tree traversal, stack based search, recursion traversal");

    concept_index_add(idx, 8, "Breadth First Search",
        "Graph traversal algorithm that explores all neighbor nodes at the present depth before moving to the next level",
        "bfs, level order traversal, queue search, unweighted shortest path");

    concept_index_add(idx, 9, "Hash Table",
        "Data structure that implements an associative array abstract data type, mapping keys to values using a hash function",
        "hash map, dictionary, separate chaining, key value store");

    concept_index_add(idx, 10, "Min Heap",
        "A complete binary tree data structure where the parent node is always less than or equal to its children",
        "priority queue, binary heap, top-k ranking, heap sort");
}

/* Search concepts by descriptive phrasing */
size_t concept_index_search(const ConceptIndex *idx, const char *query_description, size_t limit, ConceptSearchResult **out_results) {
    if (!idx || !query_description || !out_results || limit == 0) {
        if (out_results) *out_results = NULL;
        return 0;
    }

    char tokens[MAX_TOKENS_PER_QUERY][64];
    size_t token_count = tokenize_and_filter(query_description, tokens);
    if (token_count == 0) {
        *out_results = NULL;
        return 0;
    }

    /* Accumulator for scores indexed by concept_id */
    double scores[MAX_CONCEPTS + 1] = { 0.0 };
    size_t match_counts[MAX_CONCEPTS + 1] = { 0 };

    /* For each query token, find postings */
    for (size_t t = 0; t < token_count; t++) {
        for (size_t i = 0; i < idx->index_size; i++) {
            if (strcmp(idx->index_entries[i].token, tokens[t]) == 0) {
                PostingNode *p = idx->index_entries[i].postings;
                while (p) {
                    if (p->concept_id <= MAX_CONCEPTS) {
                        scores[p->concept_id] += p->weight;
                        match_counts[p->concept_id]++;
                    }
                    p = p->next;
                }
                break;
            }
        }
    }

    /* Collect candidate concepts */
    ConceptSearchResult *candidates = (ConceptSearchResult *)malloc(idx->concept_count * sizeof(ConceptSearchResult));
    size_t candidate_count = 0;

    for (size_t c = 0; c < idx->concept_count; c++) {
        int cid = idx->concepts[c].id;
        if (cid <= MAX_CONCEPTS && scores[cid] > 0.0) {
            ConceptSearchResult *res = &candidates[candidate_count++];
            strncpy(res->concept_name, idx->concepts[c].concept_name, sizeof(res->concept_name) - 1);
            res->concept_name[sizeof(res->concept_name) - 1] = '\0';

            strncpy(res->description, idx->concepts[c].description, sizeof(res->description) - 1);
            res->description[sizeof(res->description) - 1] = '\0';

            res->keyword_count = idx->concepts[c].keyword_count;
            for (size_t k = 0; k < res->keyword_count; k++) {
                strncpy(res->related_keywords[k], idx->concepts[c].related_keywords[k], 127);
                res->related_keywords[k][127] = '\0';
            }

            /* Normalize relevance score between 0.0 and 1.0 */
            /* Boosted by fraction of matched query tokens */
            double token_coverage = (double)match_counts[cid] / (double)token_count;
            double raw_score = scores[cid] * (1.0 + token_coverage);
            res->relevance_score = (raw_score > 20.0) ? 0.99 : (raw_score / 20.0);
            if (res->relevance_score < 0.1) res->relevance_score = 0.1;
            res->matched_token_count = match_counts[cid];
        }
    }

    if (candidate_count == 0) {
        free(candidates);
        *out_results = NULL;
        return 0;
    }

    /* Sort candidates by relevance score descending */
    for (size_t i = 0; i < candidate_count - 1; i++) {
        for (size_t j = i + 1; j < candidate_count; j++) {
            if (candidates[j].relevance_score > candidates[i].relevance_score) {
                ConceptSearchResult temp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = temp;
            }
        }
    }

    size_t return_count = (limit < candidate_count) ? limit : candidate_count;
    ConceptSearchResult *final_results = (ConceptSearchResult *)malloc(return_count * sizeof(ConceptSearchResult));
    memcpy(final_results, candidates, return_count * sizeof(ConceptSearchResult));
    free(candidates);

    *out_results = final_results;
    return return_count;
}

void concept_index_free_results(ConceptSearchResult *results) {
    if (results) free(results);
}

