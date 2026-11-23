import sqlite3
import json

def update_dataset_and_db():
    # 1. Update seed_dataset.json
    with open('database/seed_dataset.json', 'r', encoding='utf-8') as f:
        data = json.load(f)

    # Add mac keywords if missing
    existing_kws = {k['keyword']: k for k in data['keywords']}
    new_kws = [
        {"keyword": "mac address", "frequency": 110},
        {"keyword": "macos", "frequency": 70},
        {"keyword": "machine code", "frequency": 45},
        {"keyword": "macro processor", "frequency": 30}
    ]
    for nk in new_kws:
        if nk['keyword'] not in existing_kws:
            data['keywords'].append(nk)

    # Refine concepts
    concept_map = {c['concept_name']: c for c in data['concepts']}
    
    # Check if Dynamic Programming exists
    if 'Dynamic Programming' not in concept_map:
        dp_concept = {
            "concept_name": "Dynamic Programming",
            "description": "An algorithmic paradigm that solves complex problems by breaking them down into overlapping subproblems and optimal substructure, caching intermediate results via memoization or tabulation.",
            "keywords": "overlapping subproblems, optimal substructure, memoization, tabulation, bottom-up approach, top-down approach, dynamic programming, state transition"
        }
        data['concepts'].append(dp_concept)

    # Refine Stack
    if 'Stack' in concept_map:
        concept_map['Stack']['keywords'] = "last in first out, LIFO, push and pop, stack data structure, call stack, undo buffer, function stack frame"

    # Refine Hash Table
    if 'Hash Table' in concept_map:
        concept_map['Hash Table']['keywords'] = "key-value lookup, fast key lookup, associative array, hash-based search, key-value storage, hash map, dictionary"

    # Refine Merge Sort
    if 'Merge Sort' in concept_map:
        concept_map['Merge Sort']['keywords'] = "divide and conquer, dividing an array, merging sorted halves, split and merge, merge-based sorting, stable sorting"

    # Refine AVL Tree & Red-Black Tree
    if 'AVL Tree' in concept_map:
        concept_map['AVL Tree']['keywords'] = "self-balancing binary search tree, tree rotations, height balanced, logarithmic depth guarantee, balanced bst"
    if 'Red-Black Tree' in concept_map:
        concept_map['Red-Black Tree']['keywords'] = "self-balancing binary search tree, color balancing, black height, java treemap, c++ std map, self balancing tree"

    # Refine JSON Web Token & OAuth 2.0
    if 'JSON Web Token' in concept_map:
        concept_map['JSON Web Token']['keywords'] = "browser authentication, jwt, stateless authentication, bearer token, signature verification, claims header, web authentication"
    if 'OAuth 2.0' in concept_map:
        concept_map['OAuth 2.0']['keywords'] = "browser authentication, authorization framework, access token, authorization code flow, delegated permissions, web authentication"

    with open('database/seed_dataset.json', 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2)
    print(f"Updated seed_dataset.json: {len(data['keywords'])} keywords, {len(data['concepts'])} concepts.")

    # 2. Update intellisuggest.db
    conn = sqlite3.connect('intellisuggest.db')
    c = conn.cursor()

    # Insert new mac keywords into keywords table
    for nk in new_kws:
        c.execute("INSERT OR IGNORE INTO keywords (keyword, frequency, is_active) VALUES (?, ?, 1)", (nk['keyword'], nk['frequency']))

    # Update or insert concepts
    # Dynamic Programming
    c.execute("SELECT id FROM concepts WHERE concept_name = 'Dynamic Programming'")
    row = c.fetchone()
    if not row:
        c.execute("""
            INSERT INTO concepts (concept_name, description)
            VALUES ('Dynamic Programming', 'An algorithmic paradigm that solves complex problems by breaking them down into overlapping subproblems and optimal substructure, caching intermediate results via memoization or tabulation.')
        """)
        dp_id = c.lastrowid
        dp_kws = ["overlapping subproblems", "optimal substructure", "memoization", "tabulation", "bottom-up approach", "top-down approach", "dynamic programming", "state transition"]
        for kw in dp_kws:
            c.execute("INSERT INTO concept_keywords (concept_id, keyword) VALUES (?, ?)", (dp_id, kw))
        print(f"Inserted Dynamic Programming concept (ID {dp_id})")

    # Update Stack
    c.execute("SELECT id FROM concepts WHERE concept_name = 'Stack'")
    row = c.fetchone()
    if row:
        s_id = row[0]
        c.execute("DELETE FROM concept_keywords WHERE concept_id = ?", (s_id,))
        s_kws = ["last in first out", "LIFO", "push and pop", "stack data structure", "call stack", "undo buffer", "function stack frame"]
        for kw in s_kws:
            c.execute("INSERT INTO concept_keywords (concept_id, keyword) VALUES (?, ?)", (s_id, kw))
        print("Updated Stack keywords")

    # Update Hash Table
    c.execute("SELECT id FROM concepts WHERE concept_name = 'Hash Table'")
    row = c.fetchone()
    if row:
        ht_id = row[0]
        c.execute("DELETE FROM concept_keywords WHERE concept_id = ?", (ht_id,))
        ht_kws = ["key-value lookup", "fast key lookup", "associative array", "hash-based search", "key-value storage", "hash map", "dictionary"]
        for kw in ht_kws:
            c.execute("INSERT INTO concept_keywords (concept_id, keyword) VALUES (?, ?)", (ht_id, kw))
        print("Updated Hash Table keywords")

    # Update Merge Sort
    c.execute("SELECT id FROM concepts WHERE concept_name = 'Merge Sort'")
    row = c.fetchone()
    if row:
        ms_id = row[0]
        c.execute("DELETE FROM concept_keywords WHERE concept_id = ?", (ms_id,))
        ms_kws = ["divide and conquer", "dividing an array", "merging sorted halves", "split and merge", "merge-based sorting", "stable sorting"]
        for kw in ms_kws:
            c.execute("INSERT INTO concept_keywords (concept_id, keyword) VALUES (?, ?)", (ms_id, kw))
        print("Updated Merge Sort keywords")

    # Update AVL Tree & Red-Black Tree
    c.execute("SELECT id FROM concepts WHERE concept_name = 'AVL Tree'")
    row = c.fetchone()
    if row:
        c.execute("INSERT OR IGNORE INTO concept_keywords (concept_id, keyword) VALUES (?, 'self-balancing binary search tree')", (row[0],))
    c.execute("SELECT id FROM concepts WHERE concept_name = 'Red-Black Tree'")
    row = c.fetchone()
    if row:
        c.execute("INSERT OR IGNORE INTO concept_keywords (concept_id, keyword) VALUES (?, 'self-balancing binary search tree')", (row[0],))

    # Update JSON Web Token & OAuth 2.0
    c.execute("SELECT id FROM concepts WHERE concept_name = 'JSON Web Token'")
    row = c.fetchone()
    if row:
        c.execute("INSERT OR IGNORE INTO concept_keywords (concept_id, keyword) VALUES (?, 'browser authentication')", (row[0],))
    c.execute("SELECT id FROM concepts WHERE concept_name = 'OAuth 2.0'")
    row = c.fetchone()
    if row:
        c.execute("INSERT OR IGNORE INTO concept_keywords (concept_id, keyword) VALUES (?, 'browser authentication')", (row[0],))

    conn.commit()
    conn.close()
    print("Database updates committed successfully.")

if __name__ == '__main__':
    update_dataset_and_db()

