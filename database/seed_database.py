#!/usr/bin/env python3
"""
IntelliSuggest Idempotent Database Seeder
Seeds local intellisuggest.db (or remote SQLiteCloud when specified) with:
1. 1,000+ programming & computer science keywords.
2. 200+ Forgotten-Name concepts and keyword mappings.
3. ~500 demo search events marked with is_demo = 1.
"""

import sqlite3
import json
import os
import sys
import random
from datetime import datetime, timedelta

DB_PATH = "intellisuggest.db"
DATASET_PATH = "database/seed_dataset.json"

def seed_local():
    if not os.path.exists(DATASET_PATH):
        print(f"[Error] Dataset file not found: {DATASET_PATH}")
        sys.exit(1)

    with open(DATASET_PATH, "r", encoding="utf-8") as f:
        data = json.load(f)

    keywords_data = data.get("keywords", [])
    concepts_data = data.get("concepts", [])

    print(f"Connecting to local SQLite database: {DB_PATH}")
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()

    # 1. Ensure schema exists
    c.executescript("""
    CREATE TABLE IF NOT EXISTS keywords (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      keyword TEXT UNIQUE NOT NULL,
      frequency INTEGER DEFAULT 0,
      created_at TEXT NOT NULL,
      last_searched_at TEXT,
      is_active INTEGER DEFAULT 1
    );
    CREATE INDEX IF NOT EXISTS idx_kw_keyword ON keywords(keyword);
    CREATE INDEX IF NOT EXISTS idx_kw_frequency ON keywords(frequency DESC);
    CREATE INDEX IF NOT EXISTS idx_kw_active ON keywords(is_active);

    CREATE TABLE IF NOT EXISTS search_history (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      keyword_id INTEGER NOT NULL,
      searched_at TEXT NOT NULL,
      is_demo INTEGER DEFAULT 0,
      FOREIGN KEY(keyword_id) REFERENCES keywords(id) ON DELETE CASCADE
    );
    CREATE INDEX IF NOT EXISTS idx_sh_keyword ON search_history(keyword_id);
    CREATE INDEX IF NOT EXISTS idx_sh_time ON search_history(searched_at DESC);
    CREATE INDEX IF NOT EXISTS idx_sh_demo ON search_history(is_demo);

    CREATE TABLE IF NOT EXISTS concepts (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      concept_name TEXT UNIQUE NOT NULL,
      description TEXT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS concept_keywords (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      concept_id INTEGER NOT NULL,
      keyword TEXT NOT NULL,
      FOREIGN KEY(concept_id) REFERENCES concepts(id) ON DELETE CASCADE
    );
    CREATE INDEX IF NOT EXISTS idx_ck_concept ON concept_keywords(concept_id);

    CREATE TABLE IF NOT EXISTS search_metrics (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      metric_name TEXT NOT NULL,
      metric_value REAL NOT NULL,
      recorded_at TEXT NOT NULL
    );
    """)

    # 2. Check and safely add is_demo to search_history if missing
    columns = [col[1] for col in c.execute("PRAGMA table_info(search_history)").fetchall()]
    if "is_demo" not in columns:
        print("[Schema Migration] Adding is_demo column to search_history...")
        c.execute("ALTER TABLE search_history ADD COLUMN is_demo INTEGER DEFAULT 0;")
        c.execute("CREATE INDEX IF NOT EXISTS idx_sh_demo ON search_history(is_demo);")

    # Record baseline counts before seeding
    initial_counts = {
        "keywords": c.execute("SELECT count(*) FROM keywords").fetchone()[0],
        "concepts": c.execute("SELECT count(*) FROM concepts").fetchone()[0],
        "concept_keywords": c.execute("SELECT count(*) FROM concept_keywords").fetchone()[0],
        "search_history": c.execute("SELECT count(*) FROM search_history").fetchone()[0],
        "real_searches": c.execute("SELECT count(*) FROM search_history WHERE is_demo = 0").fetchone()[0],
        "demo_searches": c.execute("SELECT count(*) FROM search_history WHERE is_demo = 1").fetchone()[0],
    }

    print("\n--- Baseline Record Counts Before Seeding ---")
    for tbl, count in initial_counts.items():
        print(f"  {tbl}: {count} records")

    conn.execute("BEGIN TRANSACTION")

    now = datetime.utcnow()

    # 3. Seed Keywords (Idempotent: preserves existing keywords and frequencies)
    kw_inserted = 0
    kw_skipped = 0

    existing_kw_set = {row[0]: row[1] for row in c.execute("SELECT keyword, id FROM keywords").fetchall()}

    for item in keywords_data:
        kw = item["keyword"].strip().lower()
        freq = int(item["frequency"])
        if kw in existing_kw_set:
            kw_skipped += 1
            continue

        # Spread created_at between 60 days and 10 days ago
        days_ago = random.randint(10, 60)
        created_at = (now - timedelta(days=days_ago, seconds=random.randint(0, 86400))).strftime("%Y-%m-%dT%H:%M:%SZ")

        # Spread last_searched_at between 0 and 20 days ago if frequency > 1
        last_searched_at = None
        if freq > 1:
            last_days = random.randint(0, min(days_ago, 20))
            last_searched_at = (now - timedelta(days=last_days, seconds=random.randint(0, 86400))).strftime("%Y-%m-%dT%H:%M:%SZ")

        c.execute(
            "INSERT INTO keywords(keyword, frequency, created_at, last_searched_at, is_active) VALUES (?, ?, ?, ?, 1)",
            (kw, freq, created_at, last_searched_at)
        )
        existing_kw_set[kw] = c.lastrowid
        kw_inserted += 1

    # 4. Seed Concepts & Concept Keywords (Idempotent: preserves existing concepts)
    concepts_inserted = 0
    concepts_skipped = 0
    ck_inserted = 0
    ck_skipped = 0

    existing_concepts = {row[0].strip().lower(): row[1] for row in c.execute("SELECT concept_name, id FROM concepts").fetchall()}

    for item in concepts_data:
        name = item["concept_name"].strip()
        desc = item["description"].strip()
        kws_csv = item.get("keywords", "")

        key = name.lower()
        if key in existing_concepts:
            concept_id = existing_concepts[key]
            concepts_skipped += 1
        else:
            c.execute("INSERT INTO concepts(concept_name, description) VALUES (?, ?)", (name, desc))
            concept_id = c.lastrowid
            existing_concepts[key] = concept_id
            concepts_inserted += 1

        # Populate concept_keywords
        existing_kws_for_concept = {row[0].strip().lower() for row in c.execute("SELECT keyword FROM concept_keywords WHERE concept_id = ?", (concept_id,)).fetchall()}

        if kws_csv:
            for kw_part in kws_csv.split(","):
                clean_kw = kw_part.strip().lower()
                if not clean_kw:
                    continue
                if clean_kw in existing_kws_for_concept:
                    ck_skipped += 1
                else:
                    c.execute("INSERT INTO concept_keywords(concept_id, keyword) VALUES (?, ?)", (concept_id, clean_kw))
                    existing_kws_for_concept.add(clean_kw)
                    ck_inserted += 1

    # 5. Seed Demo Search History (Target: ~500 events with is_demo = 1)
    history_inserted = 0
    history_skipped = 0

    current_demo_count = c.execute("SELECT count(*) FROM search_history WHERE is_demo = 1").fetchone()[0]

    if current_demo_count < 500:
        needed = 500 - current_demo_count
        # Select active keywords weighted by frequency for realistic queries
        kw_rows = c.execute("SELECT id, keyword, frequency FROM keywords WHERE is_active = 1").fetchall()
        # Filter into pool with weights
        kw_ids = [r[0] for r in kw_rows]
        weights = [max(1, r[2]) for r in kw_rows]

        # Generate timestamps spread over past 30 days
        demo_events = []
        for _ in range(needed):
            chosen_id = random.choices(kw_ids, weights=weights, k=1)[0]
            # Spread over last 30 days (skewed toward recent days)
            days_ago = int(random.expovariate(1/7)) # exponential decay around recent days
            if days_ago > 30:
                days_ago = random.randint(0, 30)
            event_time = (now - timedelta(days=days_ago, seconds=random.randint(0, 86400))).strftime("%Y-%m-%dT%H:%M:%SZ")
            demo_events.append((chosen_id, event_time, 1))

        # Sort chronologically so IDs increase with time
        demo_events.sort(key=lambda x: x[1])

        c.executemany("INSERT INTO search_history(keyword_id, searched_at, is_demo) VALUES (?, ?, ?)", demo_events)
        history_inserted = len(demo_events)
    else:
        history_skipped = current_demo_count

    conn.commit()

    # Final counts after seeding
    final_counts = {
        "keywords": c.execute("SELECT count(*) FROM keywords").fetchone()[0],
        "active_keywords": c.execute("SELECT count(*) FROM keywords WHERE is_active = 1").fetchone()[0],
        "concepts": c.execute("SELECT count(*) FROM concepts").fetchone()[0],
        "concept_keywords": c.execute("SELECT count(*) FROM concept_keywords").fetchone()[0],
        "total_searches": c.execute("SELECT count(*) FROM search_history").fetchone()[0],
        "real_searches": c.execute("SELECT count(*) FROM search_history WHERE is_demo = 0").fetchone()[0],
        "demo_searches": c.execute("SELECT count(*) FROM search_history WHERE is_demo = 1").fetchone()[0],
    }

    conn.close()

    print("\n====================================================")
    print("      INTELLISUGGEST LOCAL DATABASE SEED REPORT     ")
    print("====================================================")
    print(f"Keywords:          {kw_inserted} inserted, {kw_skipped} preserved/skipped (Total: {final_counts['keywords']})")
    print(f"Concepts:          {concepts_inserted} inserted, {concepts_skipped} preserved/skipped (Total: {final_counts['concepts']})")
    print(f"Concept Keywords:  {ck_inserted} inserted, {ck_skipped} preserved/skipped (Total: {final_counts['concept_keywords']})")
    print(f"Demo History:      {history_inserted} inserted (Total Demo: {final_counts['demo_searches']}, Real Searches: {final_counts['real_searches']})")
    print("====================================================\n")

if __name__ == "__main__":
    seed_local()

