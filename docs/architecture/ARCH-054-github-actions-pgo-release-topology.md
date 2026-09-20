# REF-ARCH-054: GitHub Actions Multi-Stage PGO Release Pipeline Topology

## 1. Architectural Scope & Purpose

This document specifies the architectural execution topology and lifecycle transitions of the WattCurb automated GitHub Actions release CI/CD workflow defined in `REF-REQ-077`. It formalizes the build matrix, profile persistence strategy, and artifact bundling structure.

---

## 2. Pipeline Execution Topology

```mermaid
flowchart TD
    subgraph Trigger["1. Trigger & Setup"]
        T1["Tag Push (v*) or workflow_dispatch"]
        S1["Checkout Source Tree (fetch-depth: 0)"]
        S2["Install Toolchain: GCC 14, CMake, Ninja, Qt6, libsystemd"]
        T1 --> S1 --> S2
    end

    subgraph Stage1["2. Stage 1: PGO Instrumentation"]
        C1["cmake -B build -G Ninja -DWATTCURB_PGO_STAGE=GENERATE"]
        B1["ninja -C build (Compiles with -fprofile-generate -flto=auto)"]
        S2 --> C1 --> B1
    end

    subgraph Stage2["3. Stage 2: Representative Profile Training"]
        E1["QT_QPA_PLATFORM=offscreen"]
        E2["Run unit tests: ./build/wattcurb_tests"]
        E3["Run daemon CLI profiles: -b, --detail, -s, -F, -X"]
        E4["Run tray benchmark: --benchmark 50000"]
        E5["Run dashboard benchmark: --benchmark 5000"]
        B1 --> E1 --> E2 --> E3 --> E4 --> E5
        E5 --> GCDA["Profile Data Generated (*.gcda in build/)"]
    end

    subgraph Stage3["4. Stage 3: PGO-Optimized Compilation"]
        C2["cmake -B build -DWATTCURB_PGO_STAGE=USE"]
        B2["ninja -C build (Compiles with -fprofile-use -fprofile-correction -flto=auto)"]
        V1["Validate with unit test suite"]
        GCDA --> C2 --> B2 --> V1
    end

    subgraph Stage4["5. Stage 4: Packaging & Release Distribution"]
        ST["strip --strip-all (wattcurb, tray, dashboard)"]
        PKG["tar -czvf wattcurb-vX.Y.Z-linux-x86_64.tar.gz"]
        SUM["sha256sum -> SHA256SUMS.txt"]
        REL["softprops/action-gh-release@v2 (Publish Release Assets)"]
        V1 --> ST --> PKG --> SUM --> REL
    end
```

---

## 3. Profile Alignment Strategy (Single Build Tree In-Place)

In GCC PGO workflows, `-fprofile-generate` writes `.gcda` files alongside object files based on relative or absolute paths embedded during compilation.
To eliminate filesystem path divergence or directory mismatches:
1. **Single Build Directory (`build/`)**: Both Stage 1 and Stage 3 operate inside the exact same build directory.
2. **In-Place Reconfiguration**: CMake reconfigures `build/` with `-DWATTCURB_PGO_STAGE=USE`.
3. **Correction Flag**: The `-fprofile-correction` flag handles multi-threaded counter synchronization smoothing, preventing false compilation halts.

---

## 4. Release Distribution Artifact Layout

The packaged tarball `wattcurb-${TAG}-linux-x86_64.tar.gz` contains the complete self-contained deployment:

```text
wattcurb-${TAG}-linux-x86_64/
├── bin/
│   ├── wattcurb                 # PGO-optimized resident daemon & CLI
│   ├── wattcurb-tray            # PGO-optimized StatusNotifierItem tray client
│   └── wattcurb-dashboard       # PGO-optimized KDE Plasma 6 matrix dashboard
├── systemd/
│   ├── wattcurb.service         # Root hardware profiling service
│   └── wattcurb-user.service    # User session desktop tray service
├── desktop/
│   ├── wattcurb-tray.desktop    # XDG autostart tray entry
│   └── wattcurb-dashboard.desktop # Desktop application menu entry
├── install.sh                   # One-shot system installation script
├── uninstall.sh                 # Clean zero-residual uninstallation script
├── README.md                    # Quick start documentation
└── LICENSE                      # MIT License
```

---

## 5. Security & Verification Guardrails
- **Integrity**: Every release publishes `SHA256SUMS.txt`.
- **Reproducibility**: All compile flags (`-O3`, `-flto=auto`, `-fvisibility=hidden`, `-DNDEBUG`) are strictly version-controlled in `CMakeLists.txt`.
- **Release Gating**: If any Stage 2 training step or post-PGO validation test fails, the workflow terminates immediately, preventing defective releases from being published.
