import urllib.request
import urllib.error
import json
import ssl

GATEWAY_URL = os.environ.get("SQLITECLOUD_GATEWAY_URL", "https://coko3dsroz.g5.gateway.sqlite.cloud/v2/weblite/sql")
API_KEY = os.environ.get("SQLITECLOUD_API_KEY", "")
DATABASE = "intellisuggest.sqlite"

def query_cloud(sql):
    payload = json.dumps({"database": DATABASE, "sql": sql}).encode('utf-8')
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {API_KEY}"
    }
    req = urllib.request.Request(GATEWAY_URL, data=payload, headers=headers, method="POST")
    ctx = ssl.create_default_context()
    try:
        with urllib.request.urlopen(req, context=ctx) as resp:
            return resp.status, json.loads(resp.read().decode('utf-8'))
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read().decode('utf-8'))

def main():
    print(f"Connecting to SQLiteCloud gateway: {GATEWAY_URL}")
    print(f"Target Database: {DATABASE}")

    # Inspect tables
    status, res = query_cloud("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;")
    print(f"Tables query status: {status}")
    print(f"Response: {json.dumps(res, indent=2)}")

    # Check search_history pragma table_info
    status, res = query_cloud("PRAGMA table_info(search_history);")
    print(f"\nsearch_history schema info: {res}")

    # Check table counts
    for tbl in ["keywords", "concepts", "concept_keywords", "search_history", "search_metrics"]:
        status, res = query_cloud(f"SELECT COUNT(*) FROM {tbl};")
        count = None
        if "data" in res and res["data"]:
            count = list(res["data"][0].values())[0]
        print(f"Count for '{tbl}': {count}")

if __name__ == '__main__':
    main()
