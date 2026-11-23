import urllib.request
import urllib.error
import json
import ssl
import sqlite3
import time

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
        body = e.read().decode('utf-8')
        try:
            return e.code, json.loads(body)
        except Exception:
            return e.code, {"error": body}

def escape_sql(s):
    if s is None:
        return "NULL"
    return "'" + str(s).replace("'", "''") + "'"

def main():
    print("======================================================================")
    print("       SYNCHRONIZING VERIFIED DATASET TO SQLITECLOUD PRODUCTION       ")
    print("======================================================================")
    print(f"Gateway: {GATEWAY_URL}")
    print(f"Database: {DATABASE}")

    # 1. Apply Schema Migration: is_demo column
    print("\n--- 1. Applying Schema Migration for is_demo ---")
    st, info = query_cloud("PRAGMA table_info(search_history);")
    has_demo = False
    if "data" in info:
        has_demo = any(col.get("name") == "is_demo" for col in info["data"])

    if not has_demo:
        print("Adding 'is_demo' column to search_history table in SQLiteCloud...")
        st, res = query_cloud("ALTER TABLE search_history ADD COLUMN is_demo INTEGER DEFAULT 0;")
        print(f"ALTER TABLE status: {st}, result: {res}")
        st, res = query_cloud("CREATE INDEX IF NOT EXISTS idx_sh_demo ON search_history(is_demo);")
        print(f"CREATE INDEX status: {st}, result: {res}")
    else:
        print("'is_demo' column already exists in search_history.")

    # 2. Synchronize Keywords
    print("\n--- 2. Synchronizing Keywords ---")
    local_conn = sqlite3.connect('intellisuggest.db')
    local_cur = local_conn.cursor()

    local_cur.execute("SELECT keyword, frequency, created_at, is_active FROM keywords")
    local_kws = local_cur.fetchall()
    print(f"Total local keywords to sync: {len(local_kws)}")

    batch_size = 50
    for i in range(0, len(local_kws), batch_size):
        batch = local_kws[i:i + batch_size]
        values = []
        for kw, freq, created, active in batch:
            values.append(f"({escape_sql(kw)}, {freq}, {escape_sql(created)}, {active})")
        sql = f"INSERT OR IGNORE INTO keywords (keyword, frequency, created_at, is_active) VALUES {', '.join(values)};"
        st, res = query_cloud(sql)
        if st not in (200, 201):
            print(f"[Error at batch {i}]: status {st}, res: {res}")
        time.sleep(0.05)

    print("Keyword batch synchronization complete.")

    # 3. Synchronize Concepts
    print("\n--- 3. Synchronizing Concepts ---")
    local_cur.execute("SELECT id, concept_name, description FROM concepts")
    local_concepts = local_cur.fetchall()
    print(f"Total local concepts to sync: {len(local_concepts)}")

    for cid, name, desc in local_concepts:
        # Check if concept already exists by name
        check_sql = f"SELECT id FROM concepts WHERE concept_name = {escape_sql(name)};"
        st, res = query_cloud(check_sql)
        cloud_cid = None
        if "data" in res and res["data"]:
            cloud_cid = list(res["data"][0].values())[0]

        if not cloud_cid:
            insert_sql = f"INSERT INTO concepts (concept_name, description) VALUES ({escape_sql(name)}, {escape_sql(desc)});"
            st, res = query_cloud(insert_sql)
            # Retrieve generated ID
            st, res = query_cloud(f"SELECT id FROM concepts WHERE concept_name = {escape_sql(name)};")
            if "data" in res and res["data"]:
                cloud_cid = list(res["data"][0].values())[0]

        # Sync keywords for this concept
        if cloud_cid:
            local_cur.execute("SELECT keyword FROM concept_keywords WHERE concept_id = ?", (cid,))
            c_kws = [r[0] for r in local_cur.fetchall()]
            if c_kws:
                val_strs = [f"({cloud_cid}, {escape_sql(kw)})" for kw in c_kws]
                kw_sql = f"INSERT OR IGNORE INTO concept_keywords (concept_id, keyword) VALUES {', '.join(val_strs)};"
                query_cloud(kw_sql)

    print("Concept synchronization complete.")

    # 4. Synchronize Demo Search History
    print("\n--- 4. Synchronizing Demo Search History ---")
    # Fetch map of keyword -> cloud keyword_id
    st, kw_res = query_cloud("SELECT id, keyword FROM keywords;")
    cloud_kw_map = {}
    if "data" in kw_res:
        for r in kw_res["data"]:
            cloud_kw_map[r["keyword"]] = r["id"]

    local_cur.execute("""
        SELECT k.keyword, h.searched_at, h.is_demo
        FROM search_history h
        JOIN keywords k ON h.keyword_id = k.id
        WHERE h.is_demo = 1
    """)
    demo_searches = local_cur.fetchall()
    print(f"Total local demo searches to sync: {len(demo_searches)}")

    # Check if demo searches already exist in cloud
    st, check_demo = query_cloud("SELECT COUNT(*) FROM search_history WHERE is_demo = 1;")
    existing_cloud_demo = 0
    if "data" in check_demo and check_demo["data"]:
        existing_cloud_demo = list(check_demo["data"][0].values())[0]

    if existing_cloud_demo == 0:
        for i in range(0, len(demo_searches), batch_size):
            batch = demo_searches[i:i + batch_size]
            vals = []
            for kw, searched_at, is_demo in batch:
                c_kid = cloud_kw_map.get(kw)
                if c_kid:
                    vals.append(f"({c_kid}, {escape_sql(searched_at)}, 1)")
            if vals:
                sql = f"INSERT INTO search_history (keyword_id, searched_at, is_demo) VALUES {', '.join(vals)};"
                query_cloud(sql)
            time.sleep(0.05)
        print("Demo searches synchronized.")
    else:
        print(f"Cloud already contains {existing_cloud_demo} demo search records. Skipping duplicate insertion.")

    local_conn.close()

    # 5. Final Cloud Counts Verification
    print("\n--- 5. Final Production Cloud Counts ---")
    for tbl in ["keywords", "concepts", "concept_keywords", "search_history", "search_metrics"]:
        st, res = query_cloud(f"SELECT COUNT(*) as cnt FROM {tbl};")
        count = list(res["data"][0].values())[0] if ("data" in res and res["data"]) else None
        print(f"  * {tbl}: {count}")

    st, demo_res = query_cloud("SELECT COUNT(*) as cnt FROM search_history WHERE is_demo = 1;")
    demo_cnt = list(demo_res["data"][0].values())[0] if ("data" in demo_res and demo_res["data"]) else 0
    st, real_res = query_cloud("SELECT COUNT(*) as cnt FROM search_history WHERE is_demo = 0;")
    real_cnt = list(real_res["data"][0].values())[0] if ("data" in real_res and real_res["data"]) else 0
    print(f"  * search_history breakdown: {demo_cnt} Demo searches, {real_cnt} Genuine searches")

if __name__ == '__main__':
    main()

