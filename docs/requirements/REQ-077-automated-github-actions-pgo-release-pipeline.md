# REF-REQ-077: Automated GitHub Actions Multi-Stage PGO Release CI/CD Pipeline

## 1. Overview & Business Intent

WattCurb achieves its verified ultra-low overhead (< 0.05% CPU, 1.20 IPC, 27ns ToolTip render latency) through Profile-Guided Optimization (PGO) and Link-Time Optimization (LTO). To ensure production tarballs distributed to Linux users always contain maximum hardware-tailored instruction scheduling, branch reorganization, and dead-branch elimination, the release process must be fully automated via GitHub Actions.

This requirement establishes the specifications for an automated GitHub Actions release CI/CD workflow that executes the complete 3-stage PGO pipeline on clean Ubuntu runners upon version tag push (`v*`) or manual dispatch.

---

## 2. Functional Requirements

### 2.1 Trigger Conditions & Inputs
- The pipeline MUST trigger automatically on tag pushes conforming to `v*` (e.g., `v1.0.0`, `v1.0.1`).
- The pipeline MUST support manual triggering via `workflow_dispatch` with optional parameters:
  - `release_tag`: Target semantic version string.
  - `draft`: Boolean flag to publish as a GitHub draft release.
  - `prerelease`: Boolean flag to publish as a pre-release.

### 2.2 Host Environment & Toolchain
- **Operating System**: `ubuntu-24.04` LTS.
- **Compiler**: GCC 14 with C++23 support (`-std=c++23`).
- **Build System**: CMake 3.28+, Ninja.
- **Libraries**: `libsystemd-dev`, Qt6 (`qt6-base-dev`, `qt6-declarative-dev`, `qml6-module-qtquick*`), `pkg-config`, `binutils`.

### 2.3 3-Stage PGO Build Pipeline
1. **Stage 1 (Instrumentation)**:
   - Configure CMake with `-DCMAKE_BUILD_TYPE=Release`, `-DWATTCURB_ENABLE_PGO=ON`, `-DWATTCURB_PGO_STAGE=GENERATE`, `-DWATTCURB_BUILD_TESTS=ON`, and `-DWATTCURB_BUILD_GUI=ON`.
   - Compile all binaries (`wattcurb`, `wattcurb-tray`, `wattcurb-dashboard`, `wattcurb_tests`) with `-fprofile-generate` and `-flto=auto`.
2. **Stage 2 (Representative Multi-Scenario Profile Training)**:
   - Execute test suite (`./build/wattcurb_tests`) to train core procfs parsers, attribution models, and l10n lookup matrices.
   - Execute daemon training (`./build/wattcurb -b`, `./build/wattcurb --detail`, `./build/wattcurb -s`, `./build/wattcurb -F`, `./build/wattcurb -X`).
   - Execute headless tray benchmark (`./build/wattcurb-tray --benchmark 50000`) to train hover hysteresis, LUT lookups, and rich HUD ToolTip formatting.
   - Execute headless dashboard benchmark (`./build/wattcurb-dashboard --benchmark 5000`) to train seqlock delta-gating and JSON serialization.
   - Verify non-empty `.gcda` counter files are generated across object directories.
3. **Stage 3 (PGO Compilation & Link-Time Optimization)**:
   - Reconfigure CMake in the same build directory with `-DWATTCURB_PGO_STAGE=USE`.
   - Compile all binaries with `-fprofile-use`, `-fprofile-correction`, and `-flto=auto`.
   - Execute unit tests again to guarantee zero functional regression under PGO.

### 2.4 Artifact Packaging & Security
- Strip all binaries (`strip --strip-all`) to remove debug symbols and ELF metadata.
- Bundle binaries, installation scripts (`install.sh`, `uninstall.sh`), systemd units (`wattcurb.service`, `wattcurb-user.service`), and desktop entries (`wattcurb-tray.desktop`, `wattcurb-dashboard.desktop`) into a tarball:
  `wattcurb-${TAG}-linux-x86_64.tar.gz`.
- Generate cryptographic checksum: `SHA256SUMS.txt`.

### 2.5 GitHub Release Publishing
- Automatically draft or publish the release using `softprops/action-gh-release@v2`.
- Upload `wattcurb-${TAG}-linux-x86_64.tar.gz` and `SHA256SUMS.txt`.
- Include markdown release notes detailing PGO optimization gains, physical hardware domain monitoring, and zero-cost 13-language l10n support.

---

## 3. Verification & Compliance Criteria
- Must build successfully on standard GitHub-hosted runners without requiring proprietary runners or privileged containers.
- Must execute all training benchmarks headlessly (using `QT_QPA_PLATFORM=offscreen`).
- Must produce zero compiler errors or profile mismatch warnings (`-Wno-error=coverage-mismatch` handled gracefully via `-fprofile-correction`).
