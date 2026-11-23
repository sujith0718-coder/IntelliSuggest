# IntelliSuggest

> **A High-Performance, Frequency-Ranked Autocomplete and Search Suggestion Engine with Forgotten-Name Search**
>
> *Academic Data Structures and Algorithms Project — Team 12*

---

## 1. Project Overview

**IntelliSuggest** is a full-stack, data structures and algorithms-centric search suggestion engine. Unlike conventional systems that offload prefix queries to slow database `LIKE '%query%'` queries or built-in search engines, IntelliSuggest implements custom in-memory data structures in **C** to deliver sub-microsecond autocomplete and frequency-based ranking:

1. **Prefix Autocomplete**: Custom **Trie** ($O(L)$ retrieval) supporting an alphabet of 27 positions (`a-z` and space) with input normalization.
2. **Frequency Management**: Custom **Hash Table** with separate chaining for $O(1)$ average search frequency tracking and dynamic collision handling.
3. **Top-K Ranking**: Custom **Min Heap** of size $K=10$ ($O(M \log K)$) to select the 10 most popular matching keywords without sorting the entire match set.
4. **Trie Top-10 Caching Optimization**: Precomputed Top-10 suggestion vectors embedded inside prefix nodes for $O(L + K)$ instant query responses with upward ancestor propagation on updates.
5. **Forgotten-Name Search**: A lightweight offline concept-to-keyword mapping and inverted index to help users find terms they remember by description rather than exact keyword.
6. **Persistent Storage**: **SQLite** database for durable keyword history, frequencies, and concept mappings (in-memory structures hydrate at startup; persistent updates occur upon search submission).
7. **Full-Stack Presentation**: Modern, responsive **React + TypeScript + Tailwind CSS** interface connected via a **CivetWeb** embedded REST API.

---

## 2. Project Team (Team 12)

| Register Number | Name | Primary Responsibilities |
| :--- | :--- | :--- |
| **25Z207** | Aravind Deva M | Trie implementation, node lifecycle, and prefix search |
| **25Z210** | Ashwin R | Min Heap, heapify operations, and Top-K ranking |
| **25Z256** | Santhosh Parthasarathi R | Custom Hash Table, separate chaining, and keyword management |
| **25Z271** | Sujith E | Integration, Top-10 Trie caching, Forgotten-Name Search, benchmarking, deployment |
| **25Z274** | Tharun J | React + TypeScript frontend, UI/UX, interface testing |

---

## 3. System Architecture

```text
React + TypeScript Frontend (Search, Analytics, Admin, Performance)
                    |
                    |  HTTP / REST (JSON via cJSON)
                    v
          CivetWeb HTTP Server (Port 8080 or $PORT)
                    |
                    v
         C Core Engine (In-Memory)
     +-----------------------------------------------------------+
     |                                                           |
     |   [Custom Trie] <-------------------> [Hash Table]        |
     |        |                                    |             |
     |        v                                    v             |
     |   [Top-10 Cache (Approach B)]         [Frequency Map]     |
     |        |                                                  |
     |        +----------------------------> [Min Heap (K=10)]   |
     |                                                           |
     |   [Forgotten-Name Inverted Index & Concept Scorer]        |
     +-----------------------------------------------------------+
                    |
                    v (Persistence on search submission & startup hydration)
            SQLite Database (keywords, history, concepts)
```

---

## 4. Key Data Structures and Time Complexities

| Data Structure | Role | Operation | Time Complexity | Space Complexity |
| :--- | :--- | :--- | :--- | :--- |
| **Trie** | Prefix searching | Insertion / Prefix Lookup | $O(L)$ | $O(N \cdot L \cdot \Sigma)$ |
| **Hash Table** | Frequency tracking | Lookup / Insert / Update | $O(1)$ avg, $O(N)$ worst | $O(N)$ |
| **Min Heap** | Top-$K$ ranking | Extract Min / Heapify | $O(M \log K)$ | $O(K)$ |
| **Trie Top-10 Cache** | Repeated query acceleration | Cached Prefix Lookup | $O(L + K)$ | $O(\text{cached nodes} \cdot K)$ |
| **Inverted Index** | Forgotten-Name concept search | Token lookup & scoring | $O(T \cdot D)$ | $O(\text{Vocabulary} + \text{Postings})$ |

*Where $L$ = keyword length, $N$ = total keywords, $M$ = matching keywords for prefix, $K$ = 10, $\Sigma$ = alphabet size (27: a-z and space), $T$ = query tokens, $D$ = concept postings.*

---

## 5. Empirical Performance Benchmark Results

The system was benchmarked using actual experiments across 10,000, 25,000, and 50,000 keyword datasets (measuring 5,000 queries per dataset):

| Dataset Size | Approach A (Heap) Avg | Approach B (Cache) Avg | Speedup Multiplier | Approach A P95 | Approach B P95 | Approach B QPS | Update Overhead |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **10,000 keywords** | 677.24 μs | **1.58 μs** | **427.5x** | 1,771.20 μs | **3.50 μs** | **631,305 req/s** | 8.07 ms |
| **25,000 keywords** | 1,535.96 μs | **1.92 μs** | **801.0x** | 3,160.00 μs | **3.60 μs** | **521,523 req/s** | 19.23 ms |
| **50,000 keywords** | 3,436.17 μs | **2.93 μs** | **1,174.1x** | 7,165.80 μs | **5.30 μs** | **341,691 req/s** | 40.25 ms |

### Architectural Trade-off Finding:
- **Query Latency**: Approach B achieves **sub-3 microsecond** query responses across all dataset sizes ($O(L + K)$ array copy), whereas Approach A's latency grows proportionally to $M \log K$ ($677\text{ }\mu\text{s} \to 3.4\text{ ms}$).
- **Update Latency**: When a frequency counter updates, Approach B spends $8\text{--}40\text{ ms}$ propagating the changes upward through ancestor nodes. In read-heavy autocomplete engines (e.g., 99% keystroke queries, 1% search submissions), Approach B is vastly superior.

---

## 6. Unit & Integration Test Results

163 total unit tests across 7 comprehensive test suites pass with 100% success:

- `test_trie`: **31 / 31 passed (100%)** — Lifecycle, normalization, exact search, prefix matching, subtree collection, node deletion.
- `test_hashmap`: **27 / 27 passed (100%)** — Dynamic hashing (djb2), separate chaining, collision handling, automatic rehashing, frequency increments.
- `test_heap`: **24 / 24 passed (100%)** — Sift up/down, conditional root replacement, descending extraction, equal frequency tie-breaking.
- `test_ranking`: **26 / 26 passed (100%)** — Approach A vs Approach B equivalence, Top-10 cache propagation, dynamic re-ranking.
- `test_database`: **33 / 33 passed (100%)** — Schema creation, hydration, search audit log, trending 24h calculation, expired filtering, concept CRUD.
- `test_forgotten_search`: **22 / 22 passed (100%)** — Inverted index tokenization, stopword filtering, TF-IDF scoring, concept mutations (add/update/delete).
- `test_admin_auth`: **11 / 11 passed (100%)** — ADMIN_API_KEY bearer authentication, constant-time validation, fail-closed posture, unauthorized route rejection.
- `test_ranking`: **15 / 15 passed (100%)** — 100% result agreement between Approach A and Approach B, cache propagation, keyword insertion/deletion.
- `test_database`: **20 / 20 passed (100%)** — SQLite schema creation, startup in-memory hydration, search submission logging, admin CRUD, expired keyword cleanup.
- `test_forgotten_search`: **15 / 15 passed (100%)** — Stop-word removal, concept scoring, direct phrasing, synonyms, incomplete descriptions, no-match handling.

---

## 7. Build and Run Instructions

### Prerequisites
- Clang or GCC supporting C11
- CMake 3.20+
- Node.js 18+ and npm (for frontend build)

### Step 1: Build the C Backend
```powershell
# Configure CMake
cmake -B build -G "MinGW Makefiles"

# Compile all targets (server, tests, benchmark)
cmake --build build
```

### Step 2: Build the React Frontend
```powershell
cd frontend
npm install
npm run build
cd ..
```

### Step 3: Run the Server
```powershell
# Run backend (CivetWeb will automatically serve frontend at http://localhost:8080)
.\build\backend\bin\intellisuggest.exe
```

Open your browser at **`http://localhost:8080`**.

### Step 4: Run the Test Suites
```powershell
.\build\backend\bin\test_trie.exe
.\build\backend\bin\test_hashmap.exe
.\build\backend\bin\test_heap.exe
.\build\backend\bin\test_ranking.exe
.\build\backend\bin\test_database.exe
.\build\backend\bin\test_forgotten_search.exe
```

### Step 5: Run the Benchmark Suite
```powershell
.\build\backend\bin\benchmark_engine.exe
```

---

## 8. Docker Deployment

```bash
# Build and start containerized application
docker-compose up --build -d

# Open application
http://localhost:8080
```
