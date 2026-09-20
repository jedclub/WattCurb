# REF-REQ-079: Static Evaluation & Token-Minimization Developer Harness

## 1. Overview & Operational Problem
During agentic pair-programming and autonomous iteration, running raw build commands (`ninja`), unit test runners (`wattcurb_tests`), and dumping raw terminal logs injects massive quantities of redundant terminal noise (e.g. GCC PGO profile-mismatch warnings, hundreds of progress lines, verbose assertion logs) directly into the LLM conversation context.
Because LLM agents re-submit the entire conversation history on every execution step, an 80 KB build log executed 10 times results in hundreds of thousands to millions of redundant input tokens billed to the user.

To permanently eradicate this inefficiency, this requirement establishes the **WattCurb Token-Minimization Harness & Static Evaluation System (`token_guard_harness`)**.

---

## 2. Functional Requirements

### 2.1 Raw Output Suppression & Compact Summarization
1. **빌드 요약 필터링 (Build Summarization)**:
   - When building targets (`harness.py build`), raw compiler noise (including GCC `-Wcoverage-mismatch`, `-Wmissing-profile`, and progress percentages) must be completely filtered out.
   - **On Success**: Output strictly one single line:
     `[BUILD OK] All X targets compiled successfully in Y.YYs`
   - **On Failure**: Extract and display ONLY the exact compiler error lines (file, line number, error description), capped at a maximum of 15 concise lines.
2. **테스트 및 오라클 게이트 요약 (Test & Oracle Gate Summarization)**:
   - When running unit tests (`harness.py test`), individual test pass logs must be condensed.
   - **On Success**: Output total test pass count, execution duration, and a 3-5 line summary table of key Oracle Gate latency benchmarks.
   - **On Failure**: Extract ONLY the failing assertion, file location, and stack frame.
3. **정적 분석 및 심볼 윤곽 요약 (Static Code Outline & Check)**:
   - Provide `harness.py outline <file>` to inspect class, method, and function signatures without dumping hundreds of lines of implementation code into the LLM context.
   - Provide `harness.py check` to run fast syntax, header include, and l10n completeness validation.
4. **원시 데이터 격리 원칙 (Raw Data Isolation)**:
   - Raw logs must be written to disk (`tmp/last_build.log`, `tmp/last_test.log`) for manual inspection if needed, but NEVER dumped into stdout/LLM context.

---

## 3. AGENTS.md Operational Mandate
1. **원시 빌드/테스트 명령어 직접 실행 금지**:
   - Agents are strictly prohibited from invoking verbose commands (e.g. `ninja -C build`, `./build/wattcurb_tests`) directly without the summarization harness.
   - Agents must invoke commands exclusively through `python3 scripts/harness.py build`, `python3 scripts/harness.py test`, etc.
2. **PGO 컴파일러 경고 플래그 차단**:
   - CMake configuration must enforce `-Wno-missing-profile` alongside `-Wno-error=coverage-mismatch` to prevent terminal spam at the compiler level.
