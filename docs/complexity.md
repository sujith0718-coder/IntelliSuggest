# Complexity Analysis

## 1. Time Complexity

| Operation | Standard Trie (Approach A) | Cached Trie (Approach B) | Hash Table (Average) | Hash Table (Worst) | Min Heap ($K=10$) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Insert Keyword** | $O(L)$ | $O(L \cdot K \log K)$ | $O(1)$ | $O(N)$ | N/A |
| **Prefix Lookup** | $O(L + M \log K)$ | $O(L + K)$ | N/A | N/A | $O(M \log K)$ |
| **Search Frequency** | N/A | N/A | $O(1)$ | $O(N)$ | N/A |
| **Update Frequency** | $O(1)$ (in Hash Table) | $O(L \cdot K \log K)$ (propagate) | $O(1)$ | $O(N)$ | N/A |
| **Delete Keyword** | $O(L)$ | $O(L \cdot K \log K)$ | $O(1)$ | $O(N)$ | N/A |

### Variable Definitions:
- $L$: Length of the keyword or prefix string (typically $3 \le L \le 30$).
- $N$: Total number of keywords indexed in the system.
- $M$: Number of keywords matching a given prefix in the Trie subtree ($1 \le M \le N$).
- $K$: Number of suggestions returned to the user ($K = 10$).

---

## 2. Space Complexity

| Component | Space Complexity | Description |
| :--- | :--- | :--- |
| **Trie (Basic)** | $O(N \cdot L \cdot \Sigma)$ | $\Sigma = 27$ pointer slots per node. Sparse nodes share common prefixes. |
| **Trie (Top-10 Cache)** | $O(U \cdot K)$ | Additional storage for 10 keyword references at each active prefix node $U$. |
| **Hash Table** | $O(N + B)$ | $B$ bucket array entries + $N$ linked list nodes for chaining. |
| **Min Heap** | $O(K)$ | Fixed array of 10 elements during Top-10 extraction. |
| **Inverted Index** | $O(V + P)$ | $V$ vocabulary tokens + $P$ total concept postings. |

---

## 3. Trade-Off Analysis: Approach A vs. Approach B

```text
Trade-Off Matrix:
+-------------------+---------------------------+----------------------------+
| Metric            | Approach A (Heap on Query)| Approach B (Node Caching)  |
+-------------------+---------------------------+----------------------------+
| Query Latency     | Moderate (depends on M)   | Extremely Fast (O(L + K))  |
| Update Latency    | Instant (O(1))            | Moderate (O(L * K log K))  |
| Memory Footprint  | Lower                     | Higher (Cache per node)    |
| Consistency Risk  | None (computed live)      | Requires cache propagation |
+-------------------+---------------------------+----------------------------+
```

