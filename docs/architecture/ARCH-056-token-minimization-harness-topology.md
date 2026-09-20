# REF-ARCH-056: Token-Minimization Harness & Static Evaluation Topology

## 1. Architectural Topology Overview
This architecture implements [`REF-REQ-079`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-079-static-eval-and-token-minimization-harness.md), establishing a defensive buffering barrier between terminal/compiler subsystems and the AI Agent LLM context window.

```
+-----------------------------------------------------------------------------------------+
|                                    AI Agent (LLM Context)                               |
+-----------------------------------------------------------------------------------------+
        ^                                                                  |
        | Filtered Compact Summary (<= 15 lines)                           | Subcommand
        | (Saves 95% - 99.8% Context Tokens)                               | Invocation
        |                                                                  v
+-----------------------------------------------------------------------------------------+
|                     WattCurb Token Guard Harness (scripts/harness.py)                   |
|                                                                                         |
|  +------------------------+  +--------------------------+  +-------------------------+  |
|  | Build Output Filter    |  | Test & Oracle Summarizer |  | Static Code Outline     |  |
|  | - Strips PGO warnings  |  | - Extracts pass count    |  | - AST / Regex extraction|  |
|  | - Strips ninja percent |  | - Extracts Oracle metrics|  | - Header signatures only|  |
|  | - Isolated error diff  |  | - Isolates failed assert |  | - Zero implementation   |  |
|  +------------------------+  +--------------------------+  +-------------------------+  |
+-----------------------------------------------------------------------------------------+
        |                                    |                             |
        v Raw logs written to disk           v Raw logs written to disk    v
+------------------------+           +------------------------+    +-----------------------+
|  tmp/last_build.log    |           |   tmp/last_test.log    |    | Source Code AST       |
+------------------------+           +------------------------+    +-----------------------+
        |                                    |
        v                                    v
+------------------------+           +------------------------+
| Ninja / GCC Compiler   |           | wattcurb_tests Runner  |
+------------------------+           +------------------------+
```

---

## 2. Subcommands Specification

### 2.1 `build` Subcommand
- **Execution**: Runs `ninja -C build [target]`.
- **Filtering Logic**:
  - Drops lines containing: `warning:.*profile`, `warning:.*coverage-mismatch`, `warning:.*note: the ABI`, `ninja: Entering directory`, `[x/y]`.
  - Captures error blocks: `error:`, `FAILED:`.
  - Captures execution time and output binary status.
- **Return Token Budget**:
  - Success: ~20 tokens (1 line: `[BUILD OK] 8 targets compiled in 1.42s | Output: build/wattcurb (364 KB), build/wattcurb-dashboard (232 KB)`)
  - Failure: ~150 tokens (Error lines + exact file/line context only).

### 2.2 `test` Subcommand
- **Execution**: Runs `./build/wattcurb_tests [suite]`.
- **Filtering Logic**:
  - Drops individual `[PASS]` lines unless relevant to Oracle Gate benchmarks.
  - Aggregates:
    - Total tests executed & passed.
    - Oracle Gate latency metrics: O(1) l10n latency, HistoryRingBuffer append latency, SIMD uevent parse latency, Deep Battery Drain analysis latency.
  - On failure: Extracts only the failed `assert(...)` and preceding test name.
- **Return Token Budget**:
  - Success: ~80 tokens (Pass counts + key latency table).
  - Failure: ~60 tokens (Failed test name + assertion details).

### 2.3 `outline` Subcommand
- **Execution**: Analyzes C++ header/source files (`scripts/harness.py outline <file>`).
- **Functionality**:
  - Extracts structs, classes, public methods, Q_PROPERTYs, and functions.
  - Omits internal method bodies, private variables, and long comments.
  - Replaces 500-line file views with a 30-line architectural outline.

### 2.4 `status` & `report` Subcommands
- **Execution**: Runs `/usr/local/bin/wattcurb --battery-report` and compresses tabular data if requested, or provides quick system daemon health verification in $\le 3$ lines.
