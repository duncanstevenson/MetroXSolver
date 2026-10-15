#!/usr/bin/env python3
"""
Metro X Automated Test Runner (C++ and JavaScript)
Runs test suites for both languages and reports unified results.
"""

import sys
import os
import subprocess
import shutil
import platform
import re

def print_header(title):
    print("\n" + "=" * 50)
    print(f" {title.center(48)} ")
    print("=" * 50)

def run_cpp_tests():
    print_header("RUNNING C++ TEST SUITE")
    workspace = os.path.dirname(os.path.abspath(__file__))
    is_win = platform.system() == "Windows"
    exe_name = "test_cpp.exe" if is_win else "test_cpp"
    exe_path = os.path.join(workspace, "tests", exe_name)
    src_test = os.path.join(workspace, "tests", "test_cpp.cpp")

    # 1. Compile C++ tests
    compile_cmd = ["g++", "-O3", "-std=c++20", "-I", os.path.join(workspace, "src"), src_test, "-o", exe_path]
    print(f"Compiling: {' '.join(compile_cmd)}...")
    res = subprocess.run(compile_cmd)
    if res.returncode != 0:
        print("[FAIL] C++ Compilation Failed!")
        return False

    # 2. Run C++ test executable
    print(f"Executing: {exe_path}...\n")
    res = subprocess.run([exe_path])
    if res.returncode != 0:
        print("[FAIL] C++ Test Suite Failed!")
        return False

    print("[OK] C++ Test Suite Passed!")
    return True

def run_js_tests():
    print_header("RUNNING JAVASCRIPT TEST SUITE")
    workspace = os.path.dirname(os.path.abspath(__file__))
    js_test_path = os.path.join(workspace, "tests", "test.js")
    html_test_path = os.path.join(workspace, "tests", "test_js.html")

    # 1. Check if Node.js is available
    node_bin = shutil.which("node")
    if node_bin:
        print(f"Using Node.js: {node_bin}")
        res = subprocess.run([node_bin, js_test_path])
        if res.returncode == 0:
            print("[OK] JavaScript Test Suite Passed (via Node.js)!")
            return True
        else:
            print("[FAIL] JavaScript Test Suite Failed!")
            return False

    # 2. Fallback to Headless Browser (Edge or Chrome)
    browser_candidates = [
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
        shutil.which("msedge"),
        shutil.which("chrome"),
        shutil.which("google-chrome"),
        shutil.which("chromium"),
        shutil.which("chromium-browser")
    ]
    browser_bin = next((b for b in browser_candidates if b and os.path.exists(b)), None)

    if browser_bin:
        print(f"Node.js not in PATH; using Headless Browser: {browser_bin}")
        file_url = "file:///" + os.path.abspath(html_test_path).replace("\\", "/")
        cmd = [browser_bin, "--headless", "--disable-gpu", "--dump-dom", file_url]
        res = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
        out = res.stdout

        # Extract pre output
        match = re.search(r'<pre id="output"[^>]*>(.*?)</pre>', out, re.DOTALL)
        if match:
            test_output = match.group(1).strip()
            print(test_output)
            if "Tests Passed: 11 / 11" in test_output or "FAILED" not in test_output:
                print("\n[OK] JavaScript Test Suite Passed (via Headless Browser)!")
                return True
            else:
                print("\n[FAIL] JavaScript Test Suite Failed!")
                return False
        else:
            print("Could not parse browser output:")
            print(out[:500])
            return False

    print("[WARN] Neither Node.js nor a supported browser was found to run JavaScript tests headlessly.")
    print(f"You can open '{html_test_path}' directly in any browser to verify JS tests.")
    return False

def main():
    cpp_ok = run_cpp_tests()
    js_ok = run_js_tests()

    print_header("FINAL TEST SUMMARY")
    print(f"  C++ Tests        : {'[PASSED]' if cpp_ok else '[FAILED]'}")
    print(f"  JavaScript Tests : {'[PASSED]' if js_ok else '[FAILED]'}")
    print("=" * 50)

    if cpp_ok and js_ok:
        print("\n>>> ALL TESTS PASSED ACROSS BOTH LANGUAGES! <<<\n")
        sys.exit(0)
    else:
        print("\n>>> SOME TESTS FAILED! <<<\n")
        sys.exit(1)

if __name__ == "__main__":
    main()
