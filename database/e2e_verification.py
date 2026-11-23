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

def http_delete(path, data=None, headers=None):
    url = f"{BASE_URL}{path}"
    encoded = json.dumps(data).encode('utf-8') if data else None
    hdrs = {'Content-Type': 'application/json'} if data else {}
    if headers:
        hdrs.update(headers)
    req = urllib.request.Request(url, data=encoded, headers=hdrs, method='DELETE')
    try:
        with urllib.request.urlopen(req) as resp:
            return resp.status, json.loads(resp.read().decode('utf-8'))
    except urllib.error.HTTPError as e:
        body = e.read().decode('utf-8')
        try:
            return e.code, json.loads(body)
        except Exception:
            return e.code, {"error": body}

def run_tests():
    report = {}

    print("=== Step 1: Health & Hydration Check ===")
    status, health = http_get("/api/health")
    print(f"Health status code: {status}, response: {health}")
    report["health"] = health

    status, analytics = http_get("/api/analytics")
    print(f"Analytics summary: {analytics}")
    report["analytics_initial"] = analytics

    print("\n=== Step 2: Intelligent Search Prefixes & Ranking ===")
    prefixes = ["py", "jav", "bin", "tr", "gra", "has", "doc", "mac", "net", "rea"]
    prefix_results = {}
    for p in prefixes:
        st, res = http_get(f"/api/suggestions?prefix={p}&mode=cached")
        st_heap, res_heap = http_get(f"/api/suggestions?prefix={p}&mode=heap")
        items = res.get("suggestions", [])
        # Check ordering
        freqs = [item["frequency"] for item in items]
        is_sorted = freqs == sorted(freqs, reverse=True)
        prefix_results[p] = {
            "count": res.get("count", len(items)),
            "cached_latency_us": res.get("latency_us", 0),
            "heap_latency_us": res_heap.get("latency_us", 0),
            "is_sorted_by_freq": is_sorted,
            "top_suggestions": [(x["keyword"], x["frequency"]) for x in items[:5]]
        }
        print(f"Prefix '{p}': {len(items)} suggestions (sorted={is_sorted}) -> Top 3: {items[:3]}")

    report["prefix_tests"] = prefix_results

    print("\n=== Step 2b: Search submission frequency increment & Cache update ===")
    # Test typing does not increment
    st1, res1 = http_get("/api/suggestions?prefix=zig&mode=cached")
    freq1 = res1["suggestions"][0]["frequency"] if res1["suggestions"] else 0
    # Type again
    http_get("/api/suggestions?prefix=zig&mode=cached")
    st2, res2 = http_get("/api/suggestions?prefix=zig&mode=cached")
    freq2 = res2["suggestions"][0]["frequency"] if res2["suggestions"] else 0
    no_increment_on_type = (freq1 == freq2)
    print(f"Typing test for 'zig': freq before={freq1}, freq after typing={freq2} (No increment: {no_increment_on_type})")

    # Now submit search
    st_sub, res_sub = http_post("/api/search", {"keyword": "zig"})
    time.sleep(0.5) # allow persistence queue
    st3, res3 = http_get("/api/suggestions?prefix=zig&mode=cached")
    freq3 = res3["suggestions"][0]["frequency"] if res3["suggestions"] else 0
    increment_on_submit = (freq3 == freq2 + 1)
    print(f"Submit test for 'zig': freq after submit={freq3} (Incremented by 1: {increment_on_submit})")

    # Also test heap mode to ensure cache and heap match
    st3_h, res3_h = http_get("/api/suggestions?prefix=zig&mode=heap")
    freq3_h = res3_h["suggestions"][0]["frequency"] if res3_h["suggestions"] else 0
    cache_heap_sync = (freq3 == freq3_h)
    print(f"Cache and Heap mode sync for 'zig': cached={freq3}, heap={freq3_h} (Synced: {cache_heap_sync})")

    report["frequency_tests"] = {
        "no_increment_on_type": no_increment_on_type,
        "increment_on_submit": increment_on_submit,
        "cache_heap_sync": cache_heap_sync,
        "initial_freq": freq1,
        "final_freq": freq3
    }

    print("\n=== Step 3: Forgotten-Name Search (20+ Natural Language Descriptions) ===")
    queries = [
        "shortest path algorithm",
        "prefix-based searching",
        "last in first out",
        "minimum spanning tree",
        "self-balancing binary search tree",
        "fast key-value lookup",
        "database transaction durability",
        "browser authentication",
        "network communication protocol",
        "overlapping subproblems",
        "sorting by dividing and merging",
        "finding cycles in graphs",
        "first in first out linear structure",
        "probabilistic membership testing",
        "public key asymmetric encryption",
        "distributed consensus algorithm",
        "container virtualization engine",
        "deep learning natural language model with attention",
        "hierarchical domain name resolution",
        "cryptographic secure hashing function"
    ]
    fn_results = []
    for q in queries:
        st, res = http_post("/api/forgotten-search", {"description": q})
        results = res.get("results", [])
        top_match = results[0] if results else None
        top_name = top_match["concept_name"] if top_match else "NONE"
        top_score = top_match["relevance_score"] if top_match else 0
        fn_results.append({
            "query": q,
            "top_match": top_name,
            "score": top_score,
            "count": len(results)
        })
        print(f"Query: '{q}' -> Top: {top_name} (Score: {top_score:.1f}%)")

    report["forgotten_name_tests"] = fn_results

    print("\n=== Step 4: Analytics Verification ===")
    st, ana = http_get("/api/analytics")
    st, tr = http_get("/api/analytics/trending")
    st, hist = http_get("/api/history")
    
    # Check demo records in history
    hist_items = hist.get("history", [])
    demo_count_hist = sum(1 for h in hist_items if h.get("is_demo") is True)
    real_count_hist = sum(1 for h in hist_items if not h.get("is_demo"))

    analytics_check = {
        "total_keywords": ana.get("total_keywords"),
        "trie_total_words": ana.get("trie_total_words"),
        "trie_total_nodes": ana.get("trie_total_nodes"),
        "total_searches": ana.get("total_searches"),
        "demo_searches": ana.get("demo_searches"),
        "hash_load_factor": ana.get("hash_load_factor"),
        "hash_collisions": ana.get("hash_collisions"),
        "pending_writes": ana.get("pending_writes"),
        "persisted_writes": ana.get("persisted_writes"),
        "failed_writes": ana.get("failed_writes"),
        "trending_count": len(tr.get("trending", [])),
        "history_total_retrieved": len(hist_items),
        "history_demo_count": demo_count_hist,
        "history_real_count": real_count_hist
    }
    print(f"Analytics Verification: {json.dumps(analytics_check, indent=2)}")
    report["analytics_verification"] = analytics_check

    print("\n=== Step 5: Admin API & Security Verification ===")
    auth_header = {"Authorization": f"Bearer {ADMIN_KEY}"}
    
    # 5.1 Unauthorized tests
    st_unauth_kw, _ = http_post("/api/keywords", {"keyword": "testunauthkeyword", "frequency": 10})
    st_unauth_del, _ = http_delete("/api/keywords", {"keyword": "python"})
    st_unauth_concept, _ = http_post("/api/concepts", {"name": "test", "description": "test", "keywords": ["t"]})

    admin_security = {
        "post_keyword_unauth_status": st_unauth_kw,
        "delete_keyword_unauth_status": st_unauth_del,
        "post_concept_unauth_status": st_unauth_concept,
        "unauth_properly_rejected": (st_unauth_kw == 401 and st_unauth_del == 401 and st_unauth_concept == 401)
    }
    print(f"Unauthorized security check: {admin_security}")

    # 5.2 Authorized Keyword CRUD
    test_kw = "autotestword"
    st_add, res_add = http_post("/api/keywords", {"keyword": test_kw, "frequency": 77}, headers=auth_header)
    print(f"Add keyword status: {st_add}, res: {res_add}")
    kw_id = res_add.get("id") if isinstance(res_add, dict) else None
    
    # Verify in trie suggestions
    st_sug, res_sug = http_get(f"/api/suggestions?prefix=autotest&mode=cached")
    found_in_sug = any(x["keyword"] == test_kw for x in res_sug.get("suggestions", []))
    print(f"Found '{test_kw}' in suggestions: {found_in_sug}")

    # Delete keyword
    if kw_id:
        st_del, res_del = http_delete(f"/api/keywords?id={kw_id}", headers=auth_header)
    else:
        st_del, res_del = 400, "Missing ID"
    print(f"Delete keyword status: {st_del}, res: {res_del}")
    st_sug2, res_sug2 = http_get(f"/api/suggestions?prefix=autotest&mode=cached")
    removed_from_sug = not any(x["keyword"] == test_kw for x in res_sug2.get("suggestions", []))
    print(f"Removed '{test_kw}' from suggestions: {removed_from_sug}")

    # 5.3 Authorized Concept CRUD
    test_c_name = "Autotest Temporary Concept"
    test_c_desc = "A temporary concept used to verify administrator API functionality."
    test_c_kws = "temporary, verification, autotest"
    
    st_c_add, res_c_add = http_post("/api/concepts", {
        "concept_name": test_c_name,
        "description": test_c_desc,
        "keywords": test_c_kws
    }, headers=auth_header)
    print(f"Add concept status: {st_c_add}, res: {res_c_add}")
    c_id = res_c_add.get("id") if isinstance(res_c_add, dict) else None

    # Verify search finds it
    st_fn, res_fn = http_post("/api/forgotten-search", {"description": "temporary concept used to verify administrator"})
    found_in_fn = any(x["concept_name"] == test_c_name for x in res_fn.get("results", []))
    print(f"Found new concept in Forgotten Search: {found_in_fn}")

    # Edit concept
    if c_id:
        st_c_edit, res_c_edit = http_put("/api/concepts", {
            "id": c_id,
            "concept_name": test_c_name + " Updated",
            "description": test_c_desc + " Now updated with new details.",
            "keywords": test_c_kws + ", updated"
        }, headers=auth_header)
        print(f"Update concept status: {st_c_edit}, res: {res_c_edit}")

        # Delete concept
        st_c_del, res_c_del = http_delete(f"/api/concepts?id={c_id}", headers=auth_header)
        print(f"Delete concept status: {st_c_del}, res: {res_c_del}")
    else:
        st_c_edit, st_c_del = 400, 400

    admin_crud = {
        "keyword_crud_success": (st_add in (200, 201) and found_in_sug and st_del == 200 and removed_from_sug),
        "concept_crud_success": (st_c_add in (200, 201) and found_in_fn and (c_id is not None) and st_c_del == 200)
    }
    print(f"Admin CRUD results: {admin_crud}")
    report["admin_tests"] = {
        "security": admin_security,
        "crud": admin_crud
    }

    # Save report
    with open("database/verification_output.json", "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)
    print("\nVerification report saved to database/verification_output.json")

if __name__ == "__main__":
    run_tests()
