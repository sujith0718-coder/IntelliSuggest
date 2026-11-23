import sqlite3
import re
from collections import defaultdict

def levenshtein_distance(s1, s2):
    if len(s1) < len(s2):
        return levenshtein_distance(s2, s1)
    if len(s2) == 0:
        return len(s1)
    previous_row = range(len(s2) + 1)
    for i, c1 in enumerate(s1):
        current_row = [i + 1]
        for j, c2 in enumerate(s2):
            insertions = previous_row[j + 1] + 1
            deletions = current_row[j] + 1
            substitutions = previous_row[j] + (c1 != c2)
            current_row.append(min(insertions, deletions, substitutions))
        previous_row = current_row
    return previous_row[-1]

def inspect_dataset():
    conn = sqlite3.connect("intellisuggest.db")
    c = conn.cursor()

    print("======================================================================")
    print("           INTELLISUGGEST DATASET QUALITY INSPECTION REPORT           ")
    print("======================================================================")

    # 1. Fetch all keywords
    c.execute("SELECT id, keyword, frequency FROM keywords ORDER BY keyword")
    kw_rows = c.fetchall()
    keywords = [r[1] for r in kw_rows]
    kw_freq = {r[1]: r[2] for r in kw_rows}
    print(f"Total Keywords analyzed: {len(keywords)}")

    # 1a. Check for exact case-insensitive duplicates
    lower_map = defaultdict(list)
    for kw in keywords:
        lower_map[kw.lower().strip()].append(kw)
    exact_dups = {k: v for k, v in lower_map.items() if len(v) > 1}
    print(f"\n1. Exact Duplicates (case-insensitive): {len(exact_dups)}")
    for k, v in exact_dups.items():
        print(f"   - '{k}': occurrences -> {v}")

    # 1b. Near-duplicates (Edit distance <= 1 or plural forms)
    print("\n2. Near-duplicates and Variant Inspection:")
    near_duplicates = []
    # Sample common patterns (plural 's', hyphens vs spaces)
    for i in range(len(keywords)):
        w1 = keywords[i]
        for j in range(i + 1, min(i + 50, len(keywords))):
            w2 = keywords[j]
            # Check singular/plural
            if (w1 + "s" == w2) or (w1 + "es" == w2):
                near_duplicates.append((w1, w2, "Plural variation", kw_freq[w1], kw_freq[w2]))
            elif w1.replace(" ", "-") == w2 or w1.replace("-", " ") == w2:
                near_duplicates.append((w1, w2, "Hyphenation variation", kw_freq[w1], kw_freq[w2]))
            elif len(w1) > 4 and len(w2) > 4 and levenshtein_distance(w1, w2) == 1 and abs(len(w1) - len(w2)) == 0:
                near_duplicates.append((w1, w2, "Edit distance 1", kw_freq[w1], kw_freq[w2]))

    print(f"   Found {len(near_duplicates)} potential near-duplicate / variant pairs:")
    for w1, w2, reason, f1, f2 in near_duplicates[:25]:
        print(f"   - '{w1}' (freq: {f1}) vs '{w2}' (freq: {f2}) -> {reason}")

    # 2. Check frequency anomalies
    print("\n3. Frequency Value Anomalies:")
    c.execute("SELECT keyword, frequency FROM keywords WHERE frequency <= 0")
    non_positive = c.fetchall()
    print(f"   - Keywords with frequency <= 0: {len(non_positive)}")

    c.execute("SELECT keyword, frequency FROM keywords WHERE frequency > 1000")
    extreme_high = c.fetchall()
    print(f"   - Keywords with extreme high frequency (> 1000): {len(extreme_high)}")

    # 3. Check for unusual characters / potential misspellings
    print("\n4. Syntax / Formatting Anomalies in Keywords:")
    syntax_issues = []
    for kw in keywords:
        if not re.match(r'^[a-zA-Z0-9\s\+\#\.\-\_\/]+$', kw):
            syntax_issues.append(kw)
        if kw.strip() != kw:
            syntax_issues.append(f"'{kw}' (leading/trailing whitespace)")
        if "  " in kw:
            syntax_issues.append(f"'{kw}' (double space)")
    print(f"   - Syntax/whitespace anomalies: {len(syntax_issues)}")
    for s in syntax_issues:
        print(f"     * {s}")

    # 4. Inspect Concepts
    print("\n5. Concept Coverage and Quality Inspection:")
    c.execute("SELECT id, concept_name, description FROM concepts ORDER BY id")
    concepts = c.fetchall()
    print(f"   - Total Concepts analyzed: {len(concepts)}")

    # 4a. Check concepts with missing mappings
    c.execute("""
        SELECT c.id, c.concept_name, COUNT(ck.id) as kw_count
        FROM concepts c
        LEFT JOIN concept_keywords ck ON c.id = ck.concept_id
        GROUP BY c.id
        HAVING kw_count = 0
    """)
    unmapped_concepts = c.fetchall()
    print(f"   - Concepts with ZERO keyword mappings: {len(unmapped_concepts)}")
    for u in unmapped_concepts:
        print(f"     * [ID {u[0]}] {u[1]}")

    # 4b. Check concepts with few mappings (< 2)
    c.execute("""
        SELECT c.id, c.concept_name, COUNT(ck.id) as kw_count
        FROM concepts c
        JOIN concept_keywords ck ON c.id = ck.concept_id
        GROUP BY c.id
        HAVING kw_count < 2
    """)
    few_mapped = c.fetchall()
    print(f"   - Concepts with only 1 keyword mapping: {len(few_mapped)}")

    # 4c. Check description length and quality
    short_descriptions = []
    generic_descriptions = []
    for c_id, name, desc in concepts:
        if not desc or len(desc.strip()) < 30:
            short_descriptions.append((c_id, name, desc))
        if desc and any(phrase in desc.lower() for phrase in ["placeholder", "lorem ipsum", "todo", "test concept"]):
            generic_descriptions.append((c_id, name, desc))

    print(f"   - Concepts with very short descriptions (< 30 chars): {len(short_descriptions)}")
    for s in short_descriptions:
        print(f"     * [ID {s[0]}] {s[1]}: '{s[2]}'")
    print(f"   - Concepts with generic / placeholder terms: {len(generic_descriptions)}")

    conn.close()

if __name__ == "__main__":
    inspect_dataset()
