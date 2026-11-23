import os
import sys
import time
import json
import subprocess
import urllib.request
import urllib.parse
import urllib.error
import sqlite3

BASE_URL = "http://127.0.0.1:10000"
ADMIN_KEY = "test_admin_secret_key_12345"

def http_get(path, headers=None):
    url = f"{BASE_URL}{path}"
    req = urllib.request.Request(url, headers=headers or {})
    try:
        with urllib.request.urlopen(req) as resp:
            return resp.status, json.loads(resp.read().decode('utf-8'))
    except urllib.error.HTTPError as e:
        body = e.read().decode('utf-8')
        try:
            return e.code, json.loads(body)
        except Exception:
            return e.code, {"error": body}

def http_post(path, data, headers=None):
    url = f"{BASE_URL}{path}"
    encoded = json.dumps(data).encode('utf-8')
    hdrs = {'Content-Type': 'application/json'}
    if headers:
        hdrs.update(headers)
    req = urllib.request.Request(url, data=encoded, headers=hdrs, method='POST')
    try:
        with urllib.request.urlopen(req) as resp:
            return resp.status, json.loads(resp.read().decode('utf-8'))
    except urllib.error.HTTPError as e:
        body = e.read().decode('utf-8')
        try:
            return e.code, json.loads(body)
        except Exception:
            return e.code, {"error": body}

def http_put(path, data, headers=None):
    url = f"{BASE_URL}{path}"
    encoded = json.dumps(data).encode('utf-8')
    hdrs = {'Content-Type': 'application/json'}
    if headers:
        hdrs.update(headers)
    req = urllib.request.Request(url, data=encoded, headers=hdrs, method='PUT')
    try:
        with urllib.request.urlopen(req) as resp:
            return resp.status, json.loads(resp.read().decode('utf-8'))
    except urllib.error.HTTPError as e:
        body = e.read().decode('utf-8')
        try:
            return e.code, json.loads(body)
        except Exception:
            return e.code, {"error": body}

def http_delete(path, headers=None):
    url = f"{BASE_URL}{path}"
    req = urllib.request.Request(url, headers=headers or {}, method='DELETE')
    try:
        with urllib.request.urlopen(req) as resp:
            return resp.status, json.loads(resp.read().decode('utf-8'))
    except urllib.error.HTTPError as e:
        body = e.read().decode('utf-8')
        try:
            return e.code, json.loads(body)
        except Exception:
            return e.code, {"error": body}

def main():
    print("======================================================================")
    print("      INTELLISUGGEST LOCAL END-TO-END VERIFICATION & REGRESSION       ")
    print("======================================================================")

    # 1. Clean any temporary test data
    conn = sqlite3.connect('intellisuggest.db')
    c = conn.cursor()
    c.execute("DELETE FROM keywords WHERE keyword LIKE '%autotest%'")
    c.execute("DELETE FROM search_history WHERE keyword_id IN (SELECT id FROM keywords WHERE keyword = 'zig')")
    c.execute("UPDATE keywords SET frequency = 27 WHERE keyword = 'zig'")
    conn.commit()
    conn.close()

    # 2. Launch server
    env = os.environ.copy()
    mingw_bin = r"C:\Users\sujit\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261117-ucrt-x86_64\bin"
    env["PATH"] = f"{mingw_bin};{env.get('PATH', '')}"
    env["ADMIN_API_KEY"] = ADMIN_KEY
    env["PORT"] = "10000"
    env["DATABASE_PATH"] = "intellisuggest.db"
    env["DATABASE_DRIVER"] = "sqlite_local"

    server_exe = os.path.abspath("build/backend/bin/intellisuggest.exe")
    proc = subprocess.Popen([server_exe, "--port", "10000", "--db", "intellisuggest.db"], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    try:
        time.sleep(3)
        if proc.poll() is not None:
            stdout, stderr = proc.communicate()
            print(f"[FATAL] Server exited with code {proc.returncode}!\n{stdout}\n{stderr}")
            sys.exit(1)

        print("[Server]: Connected and listening on port 10000.")

        # Health
        st, health = http_get("/api/health")
        print(f"[Health Check]: Status {st}, Driver: {health.get('database_driver')}")

        # Analytics initial check
        st, ana = http_get("/api/analytics")
        print(f"[Analytics Initial]: Total Keywords: {ana.get('total_keywords')}, Trie Words: {ana.get('trie_total_words')}, Total Searches: {ana.get('total_searches')}, Demo Searches: {ana.get('demo_searches')}")

        # ---------------------------------------------------------------------
        # 2. Autocomplete correctness & equivalence
        # ---------------------------------------------------------------------
        print("\n--- 2. Autocomplete Prefix Correctness & Equivalence ---")
        test_prefixes = ['mac', 'ipv', 'sha', 'http', 'word', 'ext', 'ma', 'mach', 'py', 'jav', 'bin', 'tr', 'has', 'doc', 'net']
        prefix_report = {}

        for p in test_prefixes:
            st_cached, res_cached = http_get(f"/api/suggestions?prefix={p}&mode=cached")
            st_heap, res_heap = http_get(f"/api/suggestions?prefix={p}&mode=heap")

            items_c = res_cached.get("suggestions", [])
            items_h = res_heap.get("suggestions", [])

            # Verify every item starts with prefix
            all_start_with_p = all(x["keyword"].startswith(p) for x in items_c)
            # Verify sorted desc
            freqs = [x["frequency"] for x in items_c]
            is_sorted = freqs == sorted(freqs, reverse=True)
            # Verify cached and heap equivalence
            kws_c = [x["keyword"] for x in items_c]
            kws_h = [x["keyword"] for x in items_h]
            equivalent = (kws_c == kws_h)

            prefix_report[p] = {
                "count": len(items_c),
                "cached_latency_us": res_cached.get("latency_us"),
                "heap_latency_us": res_heap.get("latency_us"),
                "all_start_with_prefix": all_start_with_p,
                "is_sorted_by_freq": is_sorted,
                "cached_heap_equivalent": equivalent,
                "suggestions": [(x["keyword"], x["frequency"]) for x in items_c]
            }

            status_str = "OK" if (all_start_with_p and is_sorted and equivalent) else "FAIL"
            print(f"[{status_str}] Prefix '{p}': {len(items_c)} matches | Cached: {res_cached.get('latency_us'):.1f} us | Heap: {res_heap.get('latency_us'):.1f} us | Equiv: {equivalent}")
            for item in items_c[:5]:
                print(f"     -> {item['keyword']} ({item['frequency']})")

        # ---------------------------------------------------------------------
        # 3. Forgotten-Name Search Regression (20 Queries)
        # ---------------------------------------------------------------------
        print("\n--- 3. Forgotten-Name Search Regression Tests (20 Queries) ---")
        regression_queries = [
            ("shortest path algorithm", "Dijkstra Algorithm"),
            ("prefix-based searching", "Trie"),
            ("last in first out", "Stack"),
            ("minimum spanning tree", "Prim's Algorithm"),
            ("self-balancing binary search tree", "AVL Tree"),
            ("fast key-value lookup", "Hash Table"),
            ("database transaction durability", "ACID Properties"),
            ("browser authentication", "JSON Web Token"),
            ("network communication protocol", "Address Resolution Protocol"),
            ("overlapping subproblems", "Dynamic Programming"),
            ("sorting by dividing and merging", "Merge Sort"),
            ("finding cycles in graphs", "Cycle Detection"),
            ("first in first out linear structure", "Queue"),
            ("probabilistic membership testing", "Bloom Filter Membership"),
            ("public key asymmetric encryption", "Public Key Cryptography"),
            ("distributed consensus algorithm", "Consensus Protocol"),
            ("container virtualization engine", "Containerization"),
            ("deep learning natural language model with attention", "Transformer Architecture"),
            ("hierarchical domain name resolution", "Domain Name System"),
            ("cryptographic secure hashing function", "Salted Password Hashing")
        ]

        fn_regression_results = []
        for q, expected in regression_queries:
            st, res = http_post("/api/forgotten-search", {"description": q})
            results = res.get("results", [])
            top3 = results[:3]

            top_name = top3[0]["concept_name"] if top3 else "NONE"
            top_score = top3[0]["relevance_score"] if top3 else 0.0

            # Semantic relevance check: does top result match expected or closely related concept?
            is_relevant = False
            if top_name == expected:
                is_relevant = True
            elif expected in ["Prim's Algorithm", "Kruskal's Algorithm"] and top_name in ["Prim's Algorithm", "Kruskal's Algorithm"]:
                is_relevant = True
            elif expected in ["AVL Tree", "Red-Black Tree"] and top_name in ["AVL Tree", "Red-Black Tree"]:
                is_relevant = True
            elif expected in ["JSON Web Token", "OAuth 2.0"] and top_name in ["JSON Web Token", "OAuth 2.0"]:
                is_relevant = True
            elif expected in ["Public Key Cryptography", "RSA Cryptosystem"] and top_name in ["Public Key Cryptography", "RSA Cryptosystem"]:
                is_relevant = True
            elif expected in ["Address Resolution Protocol", "TCP/IP Protocol Suite", "Transmission Control Protocol", "Dynamic Host Configuration Protocol"] and "Protocol" in top_name:
                is_relevant = True
            elif any(expected.lower() in t["concept_name"].lower() for t in top3):
                is_relevant = True

            fn_regression_results.append({
                "query": q,
                "expected": expected,
                "top_name": top_name,
                "top_score": top_score,
                "top3": [(t["concept_name"], round(t["relevance_score"], 3)) for t in top3],
                "semantically_relevant": is_relevant
            })

            mark = "PASS" if is_relevant else "FAIL"
            print(f"[{mark}] Query: '{q}' -> Top: {top_name} ({top_score:.1%}) [Expected: {expected}]")
            for t in top3:
                print(f"       * {t['concept_name']} (score: {t['relevance_score']:.1%})")

        # ---------------------------------------------------------------------
        # 4. Analytics & Isolation Verification
        # ---------------------------------------------------------------------
        print("\n--- 4. Analytics & Demo-Data Isolation ---")
        st, ana_final = http_get("/api/analytics")
        st, trending = http_get("/api/analytics/trending")
        st, hist = http_get("/api/history")

        hist_items = hist.get("history", [])
        demo_in_hist = sum(1 for h in hist_items if h.get("is_demo") is True)
        real_in_hist = sum(1 for h in hist_items if not h.get("is_demo"))

        print(f"Total Searches: {ana_final.get('total_searches')} (Genuine searches only, expected 1)")
        print(f"Demo Searches: {ana_final.get('demo_searches')} (Demo searches only, expected 500)")
        print(f"Trending Keywords returned: {len(trending.get('trending', []))}")
        print(f"Recent History: Total {len(hist_items)} items ({demo_in_hist} Demo, {real_in_hist} Genuine)")

        # ---------------------------------------------------------------------
        # 5. Admin Authentication & CRUD Tests
        # ---------------------------------------------------------------------
        print("\n--- 5. Admin Authentication & CRUD Verification ---")
        auth_hdr = {"Authorization": f"Bearer {ADMIN_KEY}"}

        # Unauthorized
        st_unauth, _ = http_post("/api/keywords", {"keyword": "testkw", "frequency": 10})
        st_unauth_del, _ = http_delete("/api/keywords?id=1")
        print(f"Unauthorized rejection: POST={st_unauth}, DELETE={st_unauth_del} (Expected 401)")

        # Authorized Keyword Add & Delete
        st_add, res_add = http_post("/api/keywords", {"keyword": "adminverificationword", "frequency": 55}, headers=auth_hdr)
        k_id = res_add.get("id")
        st_sug, res_sug = http_get("/api/suggestions?prefix=adminverif&mode=cached")
        kw_found = any(x["keyword"] == "adminverificationword" for x in res_sug.get("suggestions", []))
        st_del, res_del = http_delete(f"/api/keywords?id={k_id}", headers=auth_hdr)
        st_sug_del, res_sug_del = http_get("/api/suggestions?prefix=adminverif&mode=cached")
        kw_gone = not any(x["keyword"] == "adminverificationword" for x in res_sug_del.get("suggestions", []))
        print(f"Admin Keyword CRUD: Added={st_add==201}, Found in Trie={kw_found}, Deleted={st_del==200}, Removed from Trie={kw_gone}")

        # Authorized Concept Add & Delete
        st_c_add, res_c_add = http_post("/api/concepts", {
            "concept_name": "Admin Verification Concept",
            "description": "A temporary concept to test admin CRUD APIs.",
            "keywords": "admin, verification, temp"
        }, headers=auth_hdr)
        cid = res_c_add.get("id")
        st_fn, res_fn = http_post("/api/forgotten-search", {"description": "temporary concept to test admin CRUD"})
        c_found = any(x["concept_name"] == "Admin Verification Concept" for x in res_fn.get("results", []))
        st_c_del, res_c_del = http_delete(f"/api/concepts?id={cid}", headers=auth_hdr)
        print(f"Admin Concept CRUD: Added={st_c_add==201}, Found in Search={c_found}, Deleted={st_c_del==200}")

        # Clean any remaining test rows from DB directly
        c_db = sqlite3.connect('intellisuggest.db')
        c_cur = c_db.cursor()
        c_cur.execute("DELETE FROM keywords WHERE keyword LIKE '%adminverification%'")
        c_cur.execute("DELETE FROM concepts WHERE concept_name LIKE '%Admin Verification%'")
        c_cur.execute("DELETE FROM search_history WHERE keyword_id NOT IN (SELECT id FROM keywords)")
        c_db.commit()
        c_db.close()

        # Save output JSON
        full_report = {
            "health": health,
            "analytics": ana_final,
            "prefixes": prefix_report,
            "forgotten_name": fn_regression_results,
            "admin": {
                "unauthorized_rejected": (st_unauth == 401 and st_unauth_del == 401),
                "keyword_crud_pass": (st_add == 201 and kw_found and st_del == 200 and kw_gone),
                "concept_crud_pass": (st_c_add == 201 and c_found and st_c_del == 200)
            }
        }

        with open("database/full_e2e_report.json", "w", encoding="utf-8") as f:
            json.dump(full_report, f, indent=2)
        print("\nSaved full verification results to database/full_e2e_report.json")

    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
        print("[Server]: Terminated cleanly.")

if __name__ == '__main__':
    main()

