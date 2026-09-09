# WattCurb Documentation & Reference Index

This document serves as the master registry for all specifications, research documents, architectural decisions, and verification tests across the **WattCurb** project.

All documents follow the Ref-ID naming standard:
- `REF-REQ-xxx`: Functional and Non-Functional Requirements
- `REF-RES-xxx`: Research, Prior Art, Papers, and Benchmarks
- `REF-ARCH-xxx`: Architecture, Subsystem Design, and Data Structures
- `REF-TEST-xxx`: Unit Tests, Benchmarks, and Oracle Gate Specifications

---

## 1. Requirements Registry (`docs/requirements/`)

| Ref ID | Title | Status | Related Documents |
| :--- | :--- | :--- | :--- |
| [`REF-REQ-001`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md) | Physical Hardware Power Profiling Requirements | Draft | [`REF-RES-001`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-001-prior-art-and-hardware-telemetry.md) |
| [`REF-REQ-002`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#ref-req-002) | Zero-Wakeup Daemon & Singleton Execution | Draft | [`REF-ARCH-001`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-001-daemon-architecture.md) |
| [`REF-REQ-003`](file:///home/jedclub/Develop/WattCurb/docs/requirements/REQ-001-hardware-power-profiling.md#ref-req-003) | Automated Evaluation & Oracle Gate Testing | Draft | [`REF-TEST-001`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-001-daemon-architecture.md#ref-test-001) |

---

## 2. Research & Prior Art Registry (`docs/research/`)

| Ref ID | Title | Focus Area | Date |
| :--- | :--- | :--- | :--- |
| [`REF-RES-001`](file:///home/jedclub/Develop/WattCurb/docs/research/RES-001-prior-art-and-hardware-telemetry.md) | Prior Art, Academic Papers, and Hardware Energy Telemetry Survey | RAPL, Scaphandre, Kepler, TLP, Powertop, GPU/NVMe | 2026-09-10 |

---

## 3. Architecture Registry (`docs/architecture/`)

| Ref ID | Title | Scope | Status |
| :--- | :--- | :--- | :--- |
| [`REF-ARCH-001`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-001-daemon-architecture.md) | High-Level Daemon Architecture & Subsystem Specification | Overall C++23 Pipeline & Oracle Gate | Draft |
