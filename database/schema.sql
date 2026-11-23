-- ============================================================================
-- IntelliSuggest Database Schema (SQLite)
-- ============================================================================

PRAGMA foreign_keys = ON;

-- Table: keywords
-- Stores all active and historic search terms with frequency counts
CREATE TABLE IF NOT EXISTS keywords (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    keyword TEXT UNIQUE NOT NULL,
    frequency INTEGER DEFAULT 0,
    created_at TEXT NOT NULL,
    last_searched_at TEXT,
    is_active INTEGER DEFAULT 1
);

CREATE INDEX IF NOT EXISTS idx_keywords_keyword ON keywords(keyword);
CREATE INDEX IF NOT EXISTS idx_keywords_frequency ON keywords(frequency DESC);
CREATE INDEX IF NOT EXISTS idx_keywords_active ON keywords(is_active);

-- Table: search_history
-- Records each submitted user search event for analytics
CREATE TABLE IF NOT EXISTS search_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    keyword_id INTEGER NOT NULL,
    searched_at TEXT NOT NULL,
    FOREIGN KEY(keyword_id) REFERENCES keywords(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_search_history_keyword ON search_history(keyword_id);
CREATE INDEX IF NOT EXISTS idx_search_history_time ON search_history(searched_at DESC);

-- Table: concepts
-- Stores concept descriptions for Forgotten-Name Search
CREATE TABLE IF NOT EXISTS concepts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    concept_name TEXT UNIQUE NOT NULL,
    description TEXT NOT NULL
);

-- Table: concept_keywords
-- Maps concepts to related keywords and synonyms
CREATE TABLE IF NOT EXISTS concept_keywords (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    concept_id INTEGER NOT NULL,
    keyword TEXT NOT NULL,
    FOREIGN KEY(concept_id) REFERENCES concepts(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_concept_keywords_concept ON concept_keywords(concept_id);

-- Table: search_metrics
-- Records actual runtime performance measurements
CREATE TABLE IF NOT EXISTS search_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    metric_name TEXT NOT NULL,
    metric_value REAL NOT NULL,
    recorded_at TEXT NOT NULL
);

