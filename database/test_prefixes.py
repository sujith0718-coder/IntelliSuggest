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
        elif c.isspace() or c in '-_':
            if not prev_space:
                cleaned.append(' ')
                prev_space = True
    return ''.join(cleaned).strip()

def main():
    conn = sqlite3.connect('intellisuggest.db')
    c = conn.cursor()
    c.execute('SELECT keyword, frequency FROM keywords WHERE is_active = 1')
    rows = c.fetchall()
    conn.close()

    # Deduplicate normalized keywords (keep highest frequency as Trie does)
    norm_dict = {}
    for k, f in rows:
        norm = trie_normalize(k)
        if norm:
            norm_dict[norm] = max(norm_dict.get(norm, 0), f)

    norm_kws = list(norm_dict.items())

    prefixes = ['mac', 'ma', 'mach', 'py', 'jav', 'bin', 'tr', 'has', 'doc', 'net']
    print(f"Total active unique normalized keywords: {len(norm_kws)}")

    for p in prefixes:
        matching = [(k, f) for k, f in norm_kws if k.startswith(p)]
        matching.sort(key=lambda x: x[1], reverse=True)
        print(f"\nPrefix '{p}': {len(matching)} matching keywords:")
        for k, f in matching[:10]:
            print(f"   - '{k}' (freq: {f}) -> starts with '{p}': {k.startswith(p)}")

if __name__ == '__main__':
    main()

