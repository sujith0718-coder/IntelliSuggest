import sqlite3
import json

def refine():
    conn = sqlite3.connect('intellisuggest.db')
    c = conn.cursor()

    # Queue
    c.execute("SELECT id FROM concepts WHERE concept_name = 'Queue'")
    row = c.fetchone()
    if row:
        q_id = row[0]
        c.execute("INSERT OR IGNORE INTO concept_keywords (concept_id, keyword) VALUES (?, 'first in first out')", (q_id,))
        c.execute("INSERT OR IGNORE INTO concept_keywords (concept_id, keyword) VALUES (?, 'queue data structure')", (q_id,))

    # Trie
    c.execute("SELECT id FROM concepts WHERE concept_name = 'Trie'")
    row = c.fetchone()
    if row:
        t_id = row[0]
        c.execute("INSERT OR IGNORE INTO concept_keywords (concept_id, keyword) VALUES (?, 'prefix-based searching')", (t_id,))
        c.execute("INSERT OR IGNORE INTO concept_keywords (concept_id, keyword) VALUES (?, 'prefix tree')", (t_id,))

    # Dijkstra
    c.execute("SELECT id FROM concepts WHERE concept_name = 'Dijkstra Algorithm'")
    row = c.fetchone()
    if row:
        d_id = row[0]
        c.execute("INSERT OR IGNORE INTO concept_keywords (concept_id, keyword) VALUES (?, 'shortest path algorithm')", (d_id,))

    conn.commit()
    conn.close()
    print("Concept keyword refinements applied successfully.")

if __name__ == '__main__':
    refine()

