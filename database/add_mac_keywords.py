import sqlite3

def add_mac():
    conn = sqlite3.connect('intellisuggest.db')
    c = conn.cursor()
    new_kws = [
        ('mac address', 110),
        ('macos', 70),
        ('machine code', 45),
        ('macro processor', 30)
    ]
    for kw, freq in new_kws:
        c.execute("""
            INSERT OR IGNORE INTO keywords (keyword, frequency, created_at, is_active)
            VALUES (?, ?, datetime('now'), 1)
        """, (kw, freq))
    conn.commit()

    c.execute("SELECT id, keyword, frequency FROM keywords WHERE keyword LIKE 'mac%' ORDER BY frequency DESC")
    rows = c.fetchall()
    print("Keywords starting with 'mac':")
    for r in rows:
        print(f"  - ID: {r[0]}, Keyword: '{r[1]}', Freq: {r[2]}")
    conn.close()

if __name__ == '__main__':
    add_mac()

