# [REF-REQ-008] Total Elimination of Debug/Trace/Log Artifacts in Production Release

- **Ref-ID**: `REF-REQ-008`
- **Related Requirements**: [`REF-REQ-006`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-003-pgo-pmu-optimization.md), [`REF-REQ-007`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-004-resident-daemon-direct-access.md)
- **Related Architecture**: [`REF-ARCH-003`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-003-pgo-pmu-pipeline.md), [`REF-ARCH-004`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-004-resident-daemon-event-loop.md)
- **Related Research**: [`REF-RES-004`](file:///home/jedclub/Develop/WattCurb/docs/research/PGO_PMU_REPORT.md), [`REF-RES-005`](file:///home/jedclub/Develop/WattCurb/docs/research/PMU_BENCHMARKS.md)
- **Status**: Approved

---

## 1. Executive Summary & Objective

In high-efficiency systems programming and sub-milliwatt daemon design, software must not carry diagnostic bloat into production. 

Every debug format string, file path literal, assertion handler, and symbol table entry resident in the compiled ELF text or `.rodata` segment:
1. Occupies L1 Instruction Cache (L1I) and Data Cache (L1D) lines.
2. Increases TLB page translations and instruction footprint.
3. Leaks internal architecture details and creates CPU branch penalties.

This specification mandates that production release builds of **WattCurb** completely eliminate all debug, trace, log, assertion, and runtime type information (RTTI) artifacts at compile and link time.

---

## 2. Functional & Compiler Directives

### 2.1 Complete Diagnostic & Log Residue Elimination
- All logging, diagnostic prints, and verbose tracing must be guarded by compile-time zero-cost constructs (`if constexpr` or macros expanding to `((void)0)`).
- When compiled in Release mode (`-DNDEBUG`), zero format string literals, file names, or line numbers may enter `.rodata` or `.data`.

### 2.2 Assertion & Invariant Stripping
- Runtime `assert()` statements and invariant verifications are completely removed in production binaries via mandatory `-DNDEBUG`.
- Error paths in production code must return error codes or use `std::expected` / `std::optional`, avoiding exception landing pads and runtime heap formatting.

### 2.3 Binary Hardening & Dead Code Elimination
The production build system (`CMakeLists.txt`) and compilation pipeline must enforce the following flags:
- `-DNDEBUG`: Strips standard assertions.
- `-fvisibility=hidden -fvisibility-inlines-hidden`: Eliminates dynamic symbol table exports.
- `-ffunction-sections -fdata-sections`: Emits each function and data item in its own section.
- `-Wl,--gc-sections`: Linker dead-code elimination pruning all unused sections and strings.
- `-fno-rtti`: Strips C++ runtime type information tables and type descriptors.
- `strip --strip-all`: Strips ELF symbol tables and debug sections for final distribution artifacts.

### 2.4 PGO Instrumentation Purity
- PGO instrumentation counters (`-fprofile-generate`) must only exist during Stage 1 profiling runs.
- Final production binaries (Stage 2/3) must be built strictly with `-fprofile-use` and `-fprofile-correction`, ensuring zero profiler hooks or runtime profiling libraries (`libgcov`) are linked.

---

## 3. Verification & Compliance Criteria

Production artifacts staged in `output/wattcurb` must satisfy:
1. `readelf -S output/wattcurb | grep -E '\.debug|\.comment'`: Must return empty.
2. `strings output/wattcurb | grep -iE 'assert|debug|trace|\[DEBUG\]'`: Must return zero diagnostic strings.
3. `nm -C output/wattcurb`: Must have no exported symbols (`no symbols`).
4. Final stripped binary size must remain minimal (< 200 KB).
