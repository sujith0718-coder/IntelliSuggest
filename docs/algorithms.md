# Core Algorithms and Data Structures

## 1. Trie (Prefix Autocomplete)

### Structure & Node Design
Each Trie node represents a character along a prefix path. For an alphabet consisting of `a-z` and space (27 valid symbols), each node contains:
- `children[27]`: Array of pointers to subsequent child nodes.
- `is_end_of_word`: Boolean flag indicating if this node terminates a valid keyword.
- `frequency`: Stored search popularity count for the keyword ending at this node.
- `keyword`: Pointer to the full keyword string (allocated only at terminating nodes).
- `top10_cache`: An array of Top-10 cached suggestions (`KeywordItem[10]`) for Approach B.

```text
                  (Root)
                 /      \
               'p'      'd'
               /          \
             'r'          'a'
             /              \
           'o'              't'
         /  |  \              \
       'g' 'j' 'd'            'a'
       /     \    \             \
     'r'    'e'   'u'          (data) [end, freq=120]
     /        \     \
   (prog)    (proj) (prod)
```

### Character Mapping
- `'a'` through `'z'` map to indices `0` through `25` (`c - 'a'`).
- Space `' '` maps to index `26`.
- Unsupported characters are sanitized, stripped, or converted during input normalization.

## 2. Hash Table (Frequency & Metadata Management)

### Purpose & Structure
The Hash Table maps a keyword string to its unique metadata record:
- `keyword`: String key.
- `frequency`: Current frequency counter.
- `created_at`: Creation timestamp.
- `last_searched_at`: Most recent search timestamp.
- `is_active`: Activity flag for expiration logic.

### Collision Handling: Separate Chaining
- Buckets are represented as an array of linked list heads (`HashNode**`).
- When two distinct keywords produce the same hash index, the new node is prepended to the linked list at that bucket index.
- Separate chaining ensures deterministic behavior without clustering issues common in linear probing when load factors fluctuate.

## 3. Min Heap (Top-K Ranking)

### Why Min Heap of Size K?
Suppose a prefix like `"s"` matches $M = 10,000$ keywords.
- Sorting all $M$ keywords requires $O(M \log M)$ time.
- By maintaining a **Min Heap of fixed size $K = 10$**:
  1. For each matching keyword, if the heap has fewer than 10 items, insert it.
  2. If the heap already has 10 items and the new keyword has a frequency strictly greater than the root (the minimum of the current top 10), extract/replace the root and heapify down.
- Total time complexity: $O(M \log K)$. Since $K=10$ is small and constant ($\log_2 10 \approx 3.32$), this is effectively $O(M)$ operations, vastly superior to full sorting.

## 4. Trie Top-10 Caching (Approach A vs. Approach B)

- **Approach A (Dynamic Heap Extraction)**: At query time, traverse from root to the prefix node, then perform DFS on the subtree to collect all matching words and feed them through the Min Heap.
- **Approach B (Precomputed Top-10 Cache)**: Every node stores the precomputed Top-10 suggestions for its prefix. Query latency drops to $O(L + K)$. When a keyword frequency updates, only ancestors along its path are updated.
