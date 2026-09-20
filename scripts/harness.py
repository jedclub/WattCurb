#!/usr/bin/env python3
"""
WattCurb Token-Minimization Developer & Evaluation Harness
Implements REF-REQ-079 & REF-ARCH-056: Prevents raw terminal & compiler noise
from flooding the LLM conversation context, reducing token usage by > 95%.
"""

import sys
import os
import subprocess
import time
import re
import argparse
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
TMP_DIR = REPO_ROOT / "tmp"
TMP_DIR.mkdir(exist_ok=True)

def format_size(path):
    if not path.exists():
        return "N/A"
    sz = path.stat().st_size
    if sz < 1024:
        return f"{sz} B"
    elif sz < 1024 * 1024:
        return f"{sz / 1024:.1f} KB"
    else:
        return f"{sz / (1024 * 1024):.2f} MB"

def run_cmd(cmd_list, cwd=REPO_ROOT):
    start = time.perf_counter()
    proc = subprocess.run(
        cmd_list,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )
    duration = time.perf_counter() - start
    return proc.returncode, proc.stdout, duration

def cmd_build(args):
    targets = args.targets if args.targets else []
    cmd = ["ninja", "-C", "build"] + targets
    ret, output, duration = run_cmd(cmd)

    # Always persist raw log to disk
    log_file = TMP_DIR / "last_build.log"
    log_file.write_text(output)

    if ret == 0:
        wc = REPO_ROOT / "build" / "wattcurb"
        db = REPO_ROOT / "build" / "wattcurb-dashboard"
        tr = REPO_ROOT / "build" / "wattcurb-tray"
        print(f"[BUILD OK] Compiled in {duration:.2f}s | wattcurb: {format_size(wc)}, dashboard: {format_size(db)}, tray: {format_size(tr)} (Raw log: {log_file.name})")
        return 0
    else:
        print(f"[BUILD FAILED] Exit code {ret} in {duration:.2f}s (Full log: {log_file.name})")
        # Extract only meaningful error lines (strip PGO mismatch & warnings noise)
        filtered_lines = []
        for line in output.splitlines():
            line_str = line.strip()
            # Ignore harmless PGO mismatch noise
            if "warning:" in line_str and ("profile" in line_str or "coverage-mismatch" in line_str or "the ABI" in line_str):
                continue
            if "ninja: Entering directory" in line_str:
                continue
            if re.match(r"^\[\d+/\d+\]", line_str) and "FAILED:" not in line_str:
                continue
            if any(k in line_str for k in ["error:", "FAILED:", "fatal error:", "undefined reference", "collect2:"]):
                filtered_lines.append(line_str)
            elif filtered_lines and len(filtered_lines) < 20 and not line_str.startswith("ninja:"):
                filtered_lines.append(line_str)

        if not filtered_lines:
            # Fallback: last 10 lines
            filtered_lines = [l.strip() for l in output.splitlines()[-10:] if l.strip()]

        print("--- Filtered Compiler Errors (Top 15 lines max) ---")
        for l in filtered_lines[:15]:
            print(f"  {l}")
        return ret

def cmd_test(args):
    test_bin = REPO_ROOT / "build" / "wattcurb_tests"
    if not test_bin.exists():
        print(f"[TEST ERROR] Test executable not found at {test_bin}. Run build first.")
        return 1

    cmd = [str(test_bin)]
    ret, output, duration = run_cmd(cmd)

    log_file = TMP_DIR / "last_test.log"
    log_file.write_text(output)

    if ret == 0:
        # Extract Oracle Gate benchmarks & summary
        pass_count = output.count("[PASS]")
        oracle_metrics = []
        for line in output.splitlines():
            if "[ORACLE GATE]" in line or "Analysis Latency" in line or "SIMD" in line or "O(1) L10n" in line:
                cleaned = line.strip().replace("[ORACLE GATE]", "").strip()
                if cleaned and cleaned not in oracle_metrics:
                    oracle_metrics.append(cleaned)

        print(f"[TESTS PASSED] {pass_count} test suites passed in {duration:.2f}s (Raw log: {log_file.name})")
        if oracle_metrics:
            print("--- Oracle Gate Key Latency Telemetry ---")
            for m in oracle_metrics[:6]:
                print(f"  • {m}")
        return 0
    else:
        print(f"[TESTS FAILED] Exit code {ret} in {duration:.2f}s (Full log: {log_file.name})")
        # Extract failure line and assertion
        failed_lines = []
        for line in output.splitlines():
            if any(k in line for k in ["Assertion", "failed", "FAILED", "Aborted", "SIGSEGV", "core dumped"]):
                failed_lines.append(line.strip())

        if not failed_lines:
            failed_lines = [l.strip() for l in output.splitlines()[-10:] if l.strip()]

        print("--- Failure Details ---")
        for l in failed_lines[:8]:
            print(f"  {l}")
        return ret

def cmd_outline(args):
    target_file = Path(args.file)
    if not target_file.is_absolute():
        target_file = REPO_ROOT / target_file

    if not target_file.exists():
        print(f"[OUTLINE ERROR] File not found: {target_file}")
        return 1

    content = target_file.read_text(errors="replace")
    lines = content.splitlines()
    total_lines = len(lines)

    outline_items = []
    in_class = None

    for idx, line in enumerate(lines, 1):
        stripped = line.strip()
        # Classes & Structs
        m_cls = re.match(r"^(class|struct)\s+([A-Za-z0-9_]+)", stripped)
        if m_cls:
            kind, name = m_cls.groups()
            outline_items.append((idx, f"{kind} {name}"))
            in_class = name
            continue

        # Q_PROPERTY
        if "Q_PROPERTY(" in stripped:
            prop = stripped.split("Q_PROPERTY(")[1].split(")")[0]
            outline_items.append((idx, f"  Q_PROPERTY({prop})"))
            continue

        # Q_INVOKABLE or public methods
        if "Q_INVOKABLE" in stripped:
            outline_items.append((idx, f"  {stripped}"))
            continue

        # Functions / Method definitions (rough regex)
        if re.match(r"^(virtual\s+|static\s+|inline\s+|explicit\s+)?([A-Za-z0-9_:<>&*]+)\s+([A-Za-z0-9_~]+)\s*\(.*\)\s*(noexcept|const|override)?\s*[{;]?$", stripped):
            if not stripped.startswith("//") and not stripped.startswith("#"):
                outline_items.append((idx, f"  fn: {stripped}"))

    print(f"[OUTLINE] {target_file.name} ({total_lines} total lines -> {len(outline_items)} symbols extracted):")
    for lno, item in outline_items[:40]:
        print(f"  L{lno:4d}: {item}")
    if len(outline_items) > 40:
        print(f"  ... (+ {len(outline_items) - 40} more symbols omitted)")
    return 0

def cmd_check(args):
    print("[CHECK] Running fast static integrity checks...")
    issues = 0

    # 1. Check git status
    _, status_out, _ = run_cmd(["git", "status", "--porcelain"])
    modified = [l for l in status_out.splitlines() if l.strip()]
    print(f"  • Git Working Tree: {len(modified)} modified/untracked files")

    # 2. Check build targets existence
    for b in ["wattcurb", "wattcurb-dashboard", "wattcurb-tray", "wattcurb_tests"]:
        p = REPO_ROOT / "build" / b
        if not p.exists():
            print(f"  [!] Missing build artifact: {p.name}")
            issues += 1
        else:
            print(f"  • Binary {b}: {format_size(p)} (OK)")

    # 3. Check daemon service
    ret, s_out, _ = run_cmd(["systemctl", "is-active", "wattcurb.service"])
    status_str = s_out.strip()
    print(f"  • Daemon Service (wattcurb.service): {status_str}")

    if issues == 0:
        print("[CHECK OK] All static integrity checks passed.")
        return 0
    else:
        print(f"[CHECK WARN] {issues} issue(s) detected.")
        return 1

def cmd_eval(args):
    print("=== WattCurb Compact Evaluation Pipeline ===")
    b_ret = cmd_build(argparse.Namespace(targets=[]))
    if b_ret != 0:
        return b_ret
    t_ret = cmd_test(argparse.Namespace())
    if t_ret != 0:
        return t_ret
    c_ret = cmd_check(argparse.Namespace())
    return c_ret

def main():
    parser = argparse.ArgumentParser(description="WattCurb Token-Minimization Harness")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # build
    p_build = subparsers.add_parser("build", help="Build targets with compact error/status filtering")
    p_build.add_argument("targets", nargs="*", help="Optional specific targets to build")
    p_build.set_defaults(func=cmd_build)

    # test
    p_test = subparsers.add_parser("test", help="Run tests and summarize Oracle Gate benchmarks in <= 5 lines")
    p_test.set_defaults(func=cmd_test)

    # outline
    p_outline = subparsers.add_parser("outline", help="Extract class & method outline of a file without dumping code")
    p_outline.add_argument("file", help="Path to file to outline")
    p_outline.set_defaults(func=cmd_outline)

    # check
    p_check = subparsers.add_parser("check", help="Run fast static sanity checks")
    p_check.set_defaults(func=cmd_check)

    # eval
    p_eval = subparsers.add_parser("eval", help="Full automated build + test + check evaluation pipeline")
    p_eval.set_defaults(func=cmd_eval)

    args = parser.parse_args()
    return args.func(args)

if __name__ == "__main__":
    sys.exit(main())
