import urllib.request
import json
import time
import ssl
import sys

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

base_url = "https://intellisuggest.onrender.com"

print("=" * 60)
print("INTELLISUGGEST PRODUCTION DEPLOYMENT VERIFICATION")
print(f"Target: {base_url}")
print("=" * 60)

for attempt in range(1, 20):
    try:
        health_req = urllib.request.Request(
            f"{base_url}/api/health",
            headers={"User-Agent": "IntelliSuggest-Verifier"}
        )
        with urllib.request.urlopen(health_req, context=ctx, timeout=15) as resp:
            health = json.loads(resp.read().decode())
        
        analytics_req = urllib.request.Request(
            f"{base_url}/api/analytics",
            headers={"User-Agent": "IntelliSuggest-Verifier"}
        )
        with urllib.request.urlopen(analytics_req, context=ctx, timeout=15) as resp:
            analytics = json.loads(resp.read().decode())
            
        trie_words = analytics.get("trie_total_words", 0)
        total_searches = analytics.get("total_searches", 0)
        demo_searches = analytics.get("demo_searches", 0)
        db_driver = health.get("database_driver", "unknown")
        
        print(f"[{attempt:02d}] Health: {health.get('status')} | Driver: {db_driver} | Trie Words: {trie_words} | Searches: genuine={total_searches}, demo={demo_searches}")
        
        if trie_words >= 1070:
            print("\nSUCCESS: Production deployment updated with 37-way Trie and full dataset!")
            break
    except Exception as e:
        print(f"[{attempt:02d}] Waiting for deployment / response: {e}")
    time.sleep(15)

print("\nRunning comprehensive live tests on production endpoints...")

# 1. Test Suggestions for digits
test_prefixes = ["ipv", "sha", "mac", "py", "http", "ext"]
for p in test_prefixes:
    try:
        req = urllib.request.Request(f"{base_url}/api/suggestions?prefix={p}&mode=cached")
        with urllib.request.urlopen(req, context=ctx, timeout=15) as r:
            res = json.loads(r.read().decode())
            suggs = [s.get("keyword") for s in res.get("suggestions", [])]
            print(f"Prefix '{p}': {suggs[:5]}")
    except Exception as e:
        print(f"Error testing prefix '{p}': {e}")

# 2. Test Forgotten-Name Search
forgotten_queries = [
    "last in first out",
    "overlapping subproblems",
    "shortest path algorithm",
    "self-balancing binary search tree"
]
for q in forgotten_queries:
    try:
        data = json.dumps({"description": q}).encode("utf-8")
        req = urllib.request.Request(
            f"{base_url}/api/forgotten-search",
            data=data,
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req, context=ctx, timeout=15) as r:
            res = json.loads(r.read().decode())
            top = res.get("results", [])[:2]
            top_str = ", ".join([f"{x.get('concept_name')} (score={x.get('score', 0):.2f})" for x in top])
            print(f"Forgotten query '{q}': {top_str}")
    except Exception as e:
        print(f"Error testing forgotten search '{q}': {e}")

print("Verification complete.")
