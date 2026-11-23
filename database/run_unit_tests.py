import os
import subprocess

def run_tests():
    test_binaries = [
        "test_trie.exe",
        "test_hashmap.exe",
        "test_heap.exe",
        "test_ranking.exe",
        "test_database.exe",
        "test_forgotten_search.exe",
        "test_admin_auth.exe"
    ]
    
    bin_dir = os.path.abspath("build/backend/bin")
    all_passed = True
    total_passed = 0
    total_tests = 0

    for tb in test_binaries:
        exe_path = os.path.join(bin_dir, tb)
        if not os.path.exists(exe_path):
            print(f"[MISSING] {tb}")
            all_passed = False
            continue

        res = subprocess.run([exe_path], capture_output=True, text=True)
        print(f"\n==========================================")
        print(f"Running {tb} (Exit Code: {res.returncode})")
        print(f"==========================================")
        print(res.stdout)
        if res.stderr:
            print(f"Stderr: {res.stderr}")
        if res.returncode != 0:
            all_passed = False

    print(f"\nOverall Unit Test Result: {'ALL PASSED' if all_passed else 'SOME FAILED'}")

if __name__ == '__main__':
    run_tests()

