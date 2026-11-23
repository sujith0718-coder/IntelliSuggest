import os
import sys
import time
import subprocess
import urllib.request
import urllib.error

def main():
    import sqlite3
    try:
        c_db = sqlite3.connect("intellisuggest.db")
        c_cur = c_db.cursor()
        c_cur.execute("DELETE FROM keywords WHERE keyword LIKE '%autotest%'")
        c_db.commit()
        c_db.close()
    except Exception as e:
        print(f"[Cleanup Notice] {e}")

    env = os.environ.copy()
    mingw_bin = r"C:\Users\sujit\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261117-ucrt-x86_64\bin"
    env["PATH"] = f"{mingw_bin};{env.get('PATH', '')}"
    env["ADMIN_API_KEY"] = "test_admin_secret_key_12345"
    env["PORT"] = "10000"
    env["DATABASE_PATH"] = "intellisuggest.db"
    env["DATABASE_DRIVER"] = "sqlite_local"

    server_exe = os.path.abspath("build/backend/bin/intellisuggest.exe")
    print(f"[Launcher] Starting backend server: {server_exe}")
    
    proc = subprocess.Popen(
        [server_exe, "--port", "10000", "--db", "intellisuggest.db"],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    try:
        print("[Launcher] Waiting 3 seconds for backend initialization...")
        time.sleep(3)

        # Check if process died
        if proc.poll() is not None:
            stdout, stderr = proc.communicate()
            print(f"[Launcher Error] Server exited prematurely with code {proc.returncode}!")
            print(f"Stdout:\n{stdout}")
            print(f"Stderr:\n{stderr}")
            sys.exit(1)

        print("[Launcher] Server running. Verifying HTTP connectivity...")
        try:
            with urllib.request.urlopen("http://127.0.0.1:10000/api/health") as resp:
                print(f"[Launcher] Health response HTTP {resp.status}")
        except Exception as e:
            print(f"[Launcher] Health check failed: {e}")
            sys.exit(1)

        # Verify frontend index.html served
        try:
            with urllib.request.urlopen("http://127.0.0.1:10000/") as resp:
                content = resp.read().decode('utf-8', errors='ignore')
                has_html = "<!DOCTYPE html>" in content or "<html" in content or "IntelliSuggest" in content
                print(f"[Launcher] Frontend served at '/': {has_html} ({len(content)} bytes)")
        except Exception as e:
            print(f"[Launcher Warning] Frontend serving check: {e}")

        # Run E2E test suite
        print("\n[Launcher] Running End-to-End Test Suite...")
        import e2e_verification
        e2e_verification.run_tests()
        print("\n[Launcher] End-to-End Test Suite finished successfully.")

    finally:
        print("\n[Launcher] Terminating server process (PID %d)..." % proc.pid)
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
        print("[Launcher] Server shutdown complete.")

if __name__ == "__main__":
    main()
