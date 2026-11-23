# System Architecture: IntelliSuggest

## 1. High-Level Architectural Model

IntelliSuggest is designed as a hybrid in-memory search engine backed by a persistent relational database:

```text
+--------------------------------------------------------------------+
|                         CLIENT LAYER                               |
|       React + TypeScript + Tailwind CSS + Lucide Icons             |
|   (Search Page, Analytics Dashboard, Admin Panel, Performance)     |
+---------------------------------+----------------------------------+
                                  |
                                  | HTTP / REST (JSON)
                                  v
+--------------------------------------------------------------------+
|                       COMMUNICATION LAYER                          |
|         CivetWeb Embedded C HTTP Server + cJSON Parser             |
+---------------------------------+----------------------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
|                        CORE DSA C LAYER                            |
|                                                                    |
|  +----------------------+             +-------------------------+  |
|  |     Custom Trie      |             |    Custom Hash Table    |  |
|  |   (Prefix Search)    |<----------->|  (Frequency Tracking)   |  |
|  +----------+-----------+             +-------------------------+  |
|             |                                                      |
|             v                                                      |
|  +----------------------+             +-------------------------+  |
|  |  Trie Top-10 Cache   |             |   Custom Min Heap       |  |
|  | (Precomputed Top 10) |<----------->|    (Top-K Selector)     |  |
|  +----------------------+             +-------------------------+  |
|                                                                    |
|  +--------------------------------------------------------------+  |
|  |               Forgotten-Name Inverted Index                  |  |
|  |    (Tokenization, Stop-Word Filter, Relevance Scoring)       |  |
|  +--------------------------------------------------------------+  |
+---------------------------------+----------------------------------+
                                  |
                                  | Persistence & Startup Hydration
                                  v
+--------------------------------------------------------------------+
|                        PERSISTENCE LAYER                           |
|               SQLite Embedded Relational Database                  |
|    (keywords, search_history, concepts, concept_keywords)         |
+--------------------------------------------------------------------+
```

## 2. In-Memory Strategy vs. Database Role

A central requirement of search suggestion engines is **low query latency** ($< 2 \text{ ms}$).

1. **Why not query SQLite for every keystroke?**
   - Disk/B-Tree traversals, SQL parsing, execution planning, and disk I/O add latency ($5\text{--}25\text{ ms}$).
   - Prefix wildcard queries (`LIKE 'pro%'`) cannot leverage standard index lookups as efficiently as an in-memory Trie whose edges represent characters directly.

2. **The In-Memory / SQLite Division of Responsibility:**
   - **Trie + Hash Table + Heap**: Live entirely in RAM. Every keystroke query runs 100% in-memory with zero disk I/O.
   - **SQLite**: Used strictly at startup for **hydration** (loading keywords and frequencies into the Trie and Hash Table) and on **search submission** (persisting frequency updates, search history, and concept mappings).

## 3. Concurrency and Synchronization

- CivetWeb manages a worker thread pool to handle concurrent incoming HTTP requests.
- Read operations (keystroke suggestions) occur frequently, while write operations (search submission, keyword insertion/cleanup) occur intermittently.
- A clean synchronization strategy (mutex or read-write lock) will guard shared pointer traversals in the Trie and Hash Table to prevent race conditions during updates.

