import sqlite3

def stem_token(w):
    w = w.lower()
    if len(w) <= 3: return w
    if w.endswith('ing') and len(w) > 5:
        w = w[:-3]
        if len(w) > 4 and w.endswith('e'): w = w[:-1]
    elif w.endswith('ed') and len(w) > 4:
        w = w[:-2]
        if len(w) > 4 and w.endswith('e'): w = w[:-1]
    elif w.endswith('es') and len(w) > 4 and not w.endswith('sses'):
        w = w[:-2]
        if w.endswith('v'): w = w[:-1] + 'f'
    elif w.endswith('s') and len(w) > 3 and not w.endswith('ss'):
        w = w[:-1]
    if len(w) > 4 and w.endswith('e') and w != 'tree':
        w = w[:-1]
    return w

PHRASE_MAP = [
    ("last in first out", "lifo"),
    ("first in first out", "fifo"),
    ("key value", "keyvalue"),
    ("key-value", "keyvalue"),
    ("fast key", "keyvalue"),
    ("self balancing", "selfbalancing"),
    ("self-balancing", "selfbalancing"),
    ("overlapping subproblems", "overlappingsubproblems"),
    ("optimal substructure", "optimalsubstructure"),
    ("shortest path", "shortestpath"),
    ("minimum spanning tree", "spanningtree"),
    ("spanning tree", "spanningtree"),
    ("divide and conquer", "divideconquer"),
    ("dividing and merging", "dividemerge"),
    ("split and merge", "dividemerge"),
    ("browser authentication", "browserauth"),
    ("web authentication", "browserauth"),
]

STOP_WORDS = set([
    "a", "about", "above", "after", "again", "against", "all", "am", "an", "and", "any", "are",
    "as", "at", "be", "because", "been", "before", "being", "below", "between", "both", "but", "by", "can",
    "did", "do", "does", "doing", "down", "during", "each", "few", "for", "from", "further", "had", "has", "have",
    "having", "he", "her", "here", "him", "his", "how", "if", "in", "into", "is", "it", "its", "me", "more", "most",
    "my", "no", "nor", "not", "of", "off", "on", "once", "only", "or", "other", "our", "out", "over", "own", "same",
    "so", "some", "such", "than", "that", "the", "their", "them", "then", "there", "these", "they", "this", "those",
    "through", "to", "too", "under", "until", "up", "very", "was", "we", "were", "what", "when", "where", "which",
    "while", "who", "whom", "why", "with", "you", "your",
    "something", "used", "thing", "device", "technique", "method", "way", "tool"
])

def tokenize_and_stem(text):
    if not text: return []
    lower = text.lower()
    tokens = []
    for phrase, syn in PHRASE_MAP:
        if phrase in lower:
            tokens.append(syn)
    
    # replace non-alnum with space
    cleaned = ''.join(c if c.isalnum() else ' ' for c in lower)
    for tok in cleaned.split():
        if len(tok) > 1 and tok not in STOP_WORDS:
            st = stem_token(tok)
            if st not in tokens:
                tokens.append(st)
    return tokens

# Test queries
queries = [
    "shortest path algorithm",
    "prefix-based searching",
    "last in first out",
    "minimum spanning tree",
    "self-balancing binary search tree",
    "fast key-value lookup",
    "database transaction durability",
    "browser authentication",
    "network communication protocol",
    "overlapping subproblems",
    "sorting by dividing and merging",
    "finding cycles in graphs",
    "first in first out linear structure",
    "probabilistic membership testing",
    "public key asymmetric encryption",
    "distributed consensus algorithm",
    "container virtualization engine",
    "deep learning natural language model with attention",
    "hierarchical domain name resolution",
    "cryptographic secure hashing function"
]

for q in queries:
    toks = tokenize_and_stem(q)
    print(f"Query: '{q}' -> Tokens: {toks}")

