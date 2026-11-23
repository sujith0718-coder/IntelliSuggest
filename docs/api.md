# REST API Specification

The IntelliSuggest backend exposes HTTP endpoints through the embedded CivetWeb server. All payloads utilize standard JSON serialized via cJSON.

---

## 1. Autocomplete & Search Endpoints

### 1.1 Get Suggestions
- **Method**: `GET`
- **Path**: `/api/suggestions?prefix={query}&mode={cached|heap}`
- **Description**: Returns top 10 matching suggestions for the given prefix.
- **Query Parameters**:
  - `prefix` (string, required): The search prefix (e.g. `pro`).
  - `mode` (string, optional): `cached` (default) or `heap` for performance comparison.
- **Success Response (200 OK)**:
```json
{
  "prefix": "pro",
  "count": 5,
  "mode": "cached",
  "latency_us": 142,
  "suggestions": [
    { "keyword": "programming", "frequency": 1450 },
    { "keyword": "program", "frequency": 920 },
    { "keyword": "project", "frequency": 810 },
    { "keyword": "product", "frequency": 430 },
    { "keyword": "processor", "frequency": 210 }
  ]
}
```

### 1.2 Submit Search
- **Method**: `POST`
- **Path**: `/api/search`
- **Description**: Records a user search selection, increments keyword frequency, updates search history, and refreshes the Trie Top-10 cache.
- **Request Body**:
```json
{
  "keyword": "programming"
}
```
- **Success Response (200 OK)**:
```json
{
  "success": true,
  "keyword": "programming",
  "new_frequency": 1451,
  "updated_at": "2026-11-23T22:50:00Z"
}
```

### 1.3 Popular Searches
- **Method**: `GET`
- **Path**: `/api/popular`
- **Description**: Returns the overall top searched keywords.

### 1.4 Search History
- **Method**: `GET`
- **Path**: `/api/history?limit=10`
- **Description**: Returns the most recent submitted searches.

---

## 2. Forgotten-Name Search

### 2.1 Concept Search
- **Method**: `POST`
- **Path**: `/api/forgotten-search`
- **Description**: Evaluates descriptive text against the concept inverted index.
- **Request Body**:
```json
{
  "description": "technique that stores previously calculated results"
}
```
- **Success Response (200 OK)**:
```json
{
  "query": "technique that stores previously calculated results",
  "latency_us": 320,
  "results": [
    {
      "concept_name": "Memoization",
      "description": "An optimization technique that stores the results of expensive function calls.",
      "relevance_score": 0.92,
      "related_keywords": ["caching", "dynamic programming", "lookup table"]
    }
  ]
}
```

---

## 3. Administration & Analytics Endpoints

- `POST /api/keywords`: Add a new keyword.
- `PUT /api/keywords/{id}`: Modify keyword frequency or status.
- `DELETE /api/keywords/{id}`: Deactivate/delete a keyword.
- `POST /api/keywords/cleanup`: Trigger expiration cleanup (e.g., > 90 days inactive).
- `GET /api/analytics`: Aggregate statistics (total keywords, total searches, cache hit rate).
- `GET /api/performance`: Live performance benchmarking metrics.
