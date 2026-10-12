import subprocess
import sys
import os

def run():
    print("=" * 60)
    print("  Geometry Dash AI AutoPlay Test Suite")
    print("=" * 60)
    
    test_scripts = [
        "tests/test_macro.py",
        "tests/test_gamemodes.py",
        "tests/test_solver_backtrack.py"
    ]
    
    passed = 0
    for script in test_scripts:
        print(f"\n[RUNNING] {script}...")
        result = subprocess.run([sys.executable, script], capture_output=True, text=True)
        print(result.stdout.strip())
        if result.returncode == 0:
            passed += 1
        else:
            print(f"[FAIL] {script} failed with error:")
            print(result.stderr)
            sys.exit(1)
            
    print("\n" + "=" * 60)
    print(f"  All {passed}/{len(test_scripts)} Test Suites Passed Successfully!")
    print("=" * 60)

if __name__ == "__main__":
    run()
