# Academic Research & Viva Preparation

## Key Evaluation Questions & Conceptual Answers

### Q1: Why did we choose a Trie instead of a Hash Table or Array for autocomplete?
- **Array / List**: Searching for a prefix in an unsorted array takes $O(N \cdot L)$ time. In a sorted array, binary search takes $O(L \log N)$, but finding all $M$ matching keywords requires scanning the subarray.
- **Hash Table**: Hash tables require exact string matches ($O(1)$ for full words). Prefix matching requires hashing all possible prefixes or scanning every bucket, losing the $O(1)$ advantage.
- **Trie**: Traverses the input prefix character-by-character in $O(L)$ time, regardless of how large the total dictionary $N$ is. All matching candidates branch directly from that prefix node.

### Q2: What happens when two keys hash to the same bucket in our Hash Table?
- We implement **Separate Chaining**. Each bucket is the head of a singly linked list.
- When two keys produce the same hash index (a collision), the new entry is prepended to the bucket's linked list.
- Worst-case lookup in a heavily collided bucket is $O(N)$, but with a well-distributed hash function and appropriate load factor ($\alpha \le 0.75$), average search time remains $O(1)$.

### Q3: Why use a Min Heap of size 10 instead of sorting all matches?
- If a single character prefix like `"a"` matches 20,000 words, sorting all 20,000 matches using Quicksort takes $O(M \log M)$ ($20000 \times 14.3 \approx 286,000$ operations).
- Using a Min Heap of size $K=10$, we only compare each candidate against the smallest element in our top 10 (the root). Heap adjustments take $O(\log 10) \approx 3.3$ operations.
- Total complexity is $O(M \log K)$, saving over 85% of computational work.

### Q4: How does the Trie Top-10 Cache work and how do we prevent stale data?
- Each Trie node maintains a precomputed array of up to 10 `KeywordItem` structures representing the top suggestions for that prefix.
- When a search is submitted for keyword $W$, the frequency of $W$ is incremented.
- We traverse the path from the root down to the terminating node for $W$. For each ancestor node along this path, we update its Top-10 cache to reflect the new frequency and maintain heap order.
- Deletions similarly invalidate or trigger recomputation of affected ancestor caches.

### Q5: Why is SQLite used if Trie and Hash Table are in memory?
- In-memory structures are volatile; when the server restarts or crashes, all frequency updates and search histories are lost.
- SQLite provides ACID durability, persistent storage, and relational integrity.
- In-memory structures provide sub-millisecond query execution without disk bottlenecks.
