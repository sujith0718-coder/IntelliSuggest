import sqlite3
import json

def test_matches():
    conn = sqlite3.connect('intellisuggest.db')
    c = conn.cursor()
    c.execute('SELECT id, concept_name, description FROM concepts')
    concepts = c.fetchall()
    conn.close()

    print(f"Loaded {len(concepts)} concepts from database.")

if __name__ == '__main__':
    test_matches()

