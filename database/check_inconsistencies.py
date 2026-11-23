import sqlite3
from collections import defaultdict

def trie_normalize(s):
    if not s:
        return ''
    cleaned = []
    prev_space = True
    for c in s:
        if c.isalpha():
            cleaned.append(c.lower())
            prev_space = False
        elif c.isdigit():
            cleaned.append(c)
            prev_space = False
        elif c.isspace() or c in '-_/':
            if not prev_space:
                cleaned.append(' ')
                prev_space = True
    return ''.join(cleaned).strip()

def main():
    conn = sqlite3.connect('intellisuggest.db')
    c = conn.cursor()
    c.execute('SELECT id, keyword, frequency, created_at, is_active FROM keywords ORDER BY id')
    rows = c.fetchall()
    conn.close()

    print(f"Total rows in DB: {len(rows)}")

    norm_map = defaultdict(list)
    for r in rows:
        norm = trie_normalize(r[1])
        norm_map[norm].append(r)

    print(f"Total unique normalized keys: {len(norm_map)}")
    print(f"Difference (DB rows - Normalized Trie entries): {len(rows) - len(norm_map)}")

    collisions = {k: v for k, v in norm_map.items() if len(v) > 1}
    print(f"\nColliding Groups ({len(collisions)} groups):")
    total_extra_records = 0
    for k, v in collisions.items():
        total_extra_records += len(v) - 1
        print(f"\n* Normalized key: '{k}' (Count in DB: {len(v)})")
        for item in v:
            print(f"    - ID: {item[0]}, Keyword: '{item[1]}', Freq: {item[2]}, Created: {item[3]}")

    print(f"\nTotal duplicate/colliding DB rows absorbed by Trie: {total_extra_records}")

if __name__ == '__main__':
    main()

