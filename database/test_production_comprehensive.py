import urllib.request
import json
import ssl

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

base_url = "https://intellisuggest.onrender.com"

# 20 Forgotten-Name queries
queries = [
    "shortest path algorithm",
    "prefix-based searching",
    "last in first out",
    "minimum spanning tree",
    "self-balancing binary search tree",
    "fast key-value lookup",
    "database transaction guarantee",
    "divide and merge sorting",
    "distributed consensus algorithm",
    "first in first out",
    "asymmetric key encryption",
    "stateless token authentication",
    "hierarchical domain name resolution",
    "container virtualization runtime",
    "version control snapshot",
    "browser client-side storage",
    "memory management garbage collection",
    "network packet routing",
    "relational database join",
    "overlapping subproblems memoization"
]

print("=" * 70)
print("TESTING 20 FORGOTTEN-NAME SEARCH QUERIES ON PRODUCTION")
print("=" * 70)

for idx, q in enumerate(queries, 1):
    payload = json.dumps({"description": q}).encode("utf-8")
    req = urllib.request.Request(
        f"{base_url}/api/forgotten-search",
        data=payload,
        headers={"Content-Type": "application/json"}
    )
    try:
        with urllib.request.urlopen(req, context=ctx, timeout=15) as resp:
            data = json.loads(resp.read().decode())
            results = data.get("results", [])
            top1 = results[0]["concept_name"] if results else "None"
            top2 = results[1]["concept_name"] if len(results) > 1 else ""
            print(f"[{idx:02d}] '{q}' -> Top match: {top1} ({top2})")
    except Exception as e:
        print(f"[{idx:02d}] '{q}' -> ERROR: {e}")

print("\n" + "=" * 70)
print("TESTING PERFORMANCE ENDPOINT ON PRODUCTION")
print("=" * 70)
try:
    req = urllib.request.Request(f"{base_url}/api/performance")
    with urllib.request.urlopen(req, context=ctx, timeout=15) as resp:
        perf = json.loads(resp.read().decode())
        print(json.dumps(perf, indent=2))
except Exception as e:
    print(f"Performance endpoint error: {e}")

print("\n" + "=" * 70)
print("TESTING FRONTEND ROOT HTML")
print("=" * 70)
try:
    req = urllib.request.Request(f"{base_url}/")
    with urllib.request.urlopen(req, context=ctx, timeout=15) as resp:
        html = resp.read().decode("utf-8")
        print(f"Status: {resp.status}, Content Length: {len(html)}, Title present: {'IntelliSuggest' in html}")
except Exception as e:
    print(f"Frontend load error: {e}")
