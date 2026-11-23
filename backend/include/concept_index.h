#ifndef CONCEPT_INDEX_H
#define CONCEPT_INDEX_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_CONCEPT_KEYWORDS 10
#define MAX_CONCEPTS 256
#define MAX_TOKENS_PER_QUERY 32

/* Concept record */
typedef struct {
    int id;
    char concept_name[128];
    char description[512];
    char related_keywords[MAX_CONCEPT_KEYWORDS][128];
    size_t keyword_count;
} ConceptRecord;

/* Scored search result */
typedef struct {
    char concept_name[128];
    char description[512];
    char related_keywords[MAX_CONCEPT_KEYWORDS][128];
    size_t keyword_count;
    double relevance_score;
    size_t matched_token_count;
} ConceptSearchResult;

/* Inverted index posting node */
typedef struct PostingNode {
    int concept_id;
    double weight; /* Higher weight for title/keyword match, normal for description */
    struct PostingNode *next;
} PostingNode;

/* Inverted index entry for a token */
typedef struct {
    char token[64];
    PostingNode *postings;
} InvertedIndexEntry;

/* The Concept Index structure */
typedef struct {
    ConceptRecord concepts[MAX_CONCEPTS];
    size_t concept_count;

    InvertedIndexEntry *index_entries;
    size_t index_size;
    size_t index_capacity;
} ConceptIndex;

/* Lifecycle */
ConceptIndex* concept_index_create(void);
void concept_index_free(ConceptIndex *idx);

/* Adding Concepts */
bool concept_index_add(ConceptIndex *idx, int id, const char *name, const char *description, const char *keywords_csv);
void concept_index_seed_defaults(ConceptIndex *idx);

/* Tokenization & Text Preprocessing */
bool is_stop_word(const char *word);
size_t tokenize_and_filter(const char *text, char tokens[MAX_TOKENS_PER_QUERY][64]);

/* Searching */
size_t concept_index_search(const ConceptIndex *idx, const char *query_description, size_t limit, ConceptSearchResult **out_results);
void concept_index_free_results(ConceptSearchResult *results);

#endif /* CONCEPT_INDEX_H */

