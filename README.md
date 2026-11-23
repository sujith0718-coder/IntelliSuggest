# IntelliSuggest

> **A High-Performance, Frequency-Ranked Autocomplete and Search Suggestion Engine with Forgotten-Name Search**

---

## 1. Project Overview

**IntelliSuggest** is a full-stack, data structures and algorithms-centric search suggestion system. Unlike simple prefix-matching systems or database-reliant `LIKE '%query%'` lookups, IntelliSuggest implements custom in-memory data structures in **C** to deliver sub-millisecond autocomplete and popularity-based rankings:

1. **Prefix Autocomplete**: Custom **Trie** ($O(L)$ retrieval) for fast keyword search over prefixes.
2. **Frequency Management**: Custom **Hash Table** with separate chaining for $O(1)$ average search frequency tracking.
3. **Top-K Ranking**: Custom **Min Heap** of size $K=10$ ($O(M \log K)$) to select the 10 most popular keywords without sorting the entire match set.
4. **Trie Top-10 Caching**: Precomputed Top-10 suggestion vectors embedded inside prefix nodes for $O(L + K)$ instant query responses.
5. **Forgotten-Name Search**: A lightweight inverted index and concept scoring algorithm to help users find terms they remember by description rather than exact keyword.
6. **Persistent Storage**: **SQLite** database for durable keyword history, frequencies, and concept mappings.
7. **Full-Stack Presentation**: Modern, responsive **React + TypeScript + Tailwind CSS** interface connected via a **CivetWeb** REST API.

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

## 3. Architecture & Data Flow

```text
React + TypeScript Frontend (Search, Analytics, Admin, Performance)
                    |
                    |  HTTP / REST (JSON via cJSON)
                    v
          CivetWeb HTTP Server
                    |
                    v
         C Core Engine (In-Memory)
     +-----------------------------------------+
     |                                         |
     |   [Custom Trie] <---> [Hash Table]      |
     |        |                  |             |
     |        v                  v             |
     |   [Top-10 Cache]    [Frequency Map]     |
     |        |                                |
     |        +---------> [Min Heap (K=10)]    |
     |                                         |
     |   [Forgotten-Name Concept Inverted Index]|
     +-----------------------------------------+
                    |
                    v (Persistence on search submission & startup)
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

## 5. Directory Structure

```text
IntelliSuggest/
├── backend/
│   ├── include/          # C header files (.h)
│   ├── src/              # C source implementations (.c)
│   ├── tests/            # Unit & integration test suites
│   └── CMakeLists.txt    # Backend C build configuration
├── frontend/             # React + TypeScript + Tailwind CSS application
├── database/             # SQLite schema and seed datasets
├── docs/                 # Detailed architectural and algorithmic documentation
├── performance/          # Benchmarking scripts (k6) and empirical results
├── CMakeLists.txt        # Root build configuration
├── README.md             # Project documentation
└── .gitignore            # Git exclusion rules
```

---

## 6. Getting Started (Phase 0)

### Prerequisites
- C Compiler supporting C11 (GCC or Clang)
- CMake 3.20+
- GNU Make or Ninja
- Node.js 18+ and npm (for frontend)

### Build and Run (C Engine)

```bash
# Configure build with CMake
cmake -B build -S .

# Compile executable
cmake --build build

# Execute initial verification program
./build/backend/bin/intellisuggest.exe
```
