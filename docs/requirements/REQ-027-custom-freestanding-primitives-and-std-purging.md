# [REF-REQ-030] Elimination of Bloated/Unused Standard Libraries & Custom POSIX Replacement Specification

## 1. Executive Summary & Problem Definition

While WattCurb is designed for ultra-low power consumption and minimal CPU overhead, an audit of `#include <...>` directives in `src/` revealed unnecessary Standard Library dependencies that bloat the production binary text, introduce dynamic heap allocations during initialization, and link unused runtime scaffolding:

1. **Ghost / Completely Unused Includes**:
   - `<unordered_map>` in `policy/attribution_engine.cpp` (0% used; comments explicitly state all maps were replaced with flat cache-aligned buffers).
   - `<cmath>` in `src/main.cpp` and `policy/attribution_engine.cpp` (0% used; no math functions referenced).
   - `<vector>` in `core/types.hpp` (0% used; all containers in `types.hpp` use `core::FixedVector`).

2. **Heavyweight C++ Runtime Modules to Replace with Zero-Overhead Custom/POSIX Primitives**:
   - **`<filesystem>` (`std::filesystem::path`, `exists`, `directory_iterator`)**:
     - Generates dozens of dynamic heap allocations per `path` object in `HardwareProbe` (30+ path members).
     - Pulls in extensive POSIX wrapper scaffolding and `std::error_code` handling.
     - Can be replaced with single syscalls: `access(path, F_OK) == 0` and lightweight `opendir`/`readdir`/`closedir`.
   - **`<fstream>` (`std::ifstream`)**:
     - Used exclusively in `src/main.cpp` and `core/environment_profile.hpp` to read short single-line system files.
     - Introduces locale tables, virtual dispatch, and buffer management.
     - Can be replaced with stack-buffered `open()`/`read()`/`close()`.
   - **`<string>` Residue in Hot Data Structures**:
     - `HardwarePowerBreakdown` contains 5 `std::string` fields (`cpu_governor`, `gpu_pcie_link`, `nvme_status`, `wifi_status`, `aspm_policy`).
     - Prevents `HardwarePowerBreakdown` from being a pure TriviallyCopyable POD.
     - Can be replaced with `core::FixedString<32>`.

---

## 2. Technical Requirements

### 2.1 Ghost Include Purge
- **[REQ-30.1]**: Purge `<unordered_map>`, `<cmath>`, and `<vector>` wherever their symbols are completely unreferenced.

### 2.2 Custom Zero-Allocation POSIX Filesystem Helpers (`core/posix_fs.hpp`)
- **[REQ-30.2] Single-Syscall `exists`**:
  ```cpp
  [[nodiscard]] inline bool file_exists(const char* path) noexcept {
      return ::access(path, F_OK) == 0;
  }
  ```
- **[REQ-30.3] Zero-Allocation Directory Walker**:
  ```cpp
  template <typename F>
  inline void for_each_dir_entry(const char* dir_path, F&& callback) noexcept;
  ```
  Iterates over directory entries using POSIX `opendir`/`readdir` with zero heap allocation and skips `.` and `..`.
- **[REQ-30.4] Purge `<filesystem>` from Core Subsystems**:
  - Replace `std::filesystem::path` in `ProcessAnalyzer` and `HardwareProbe` with `std::string_view` or fixed-size stack buffers (`core::FixedString<128>`).

### 2.3 Lightweight POSIX File Reader
- **[REQ-30.5] Stack-Buffered `read_text_file`**:
  - Replace `std::ifstream` with a zero-allocation POSIX reader:
  ```cpp
  inline bool read_small_file(const char* path, char* buf, size_t max_len, size_t* out_len = nullptr) noexcept;
  ```

### 2.4 100% TriviallyCopyable `HardwarePowerBreakdown`
- **[REQ-30.6]**: Convert remaining `std::string` fields in `HardwarePowerBreakdown` to `core::FixedString<32>`.
- **[REQ-30.7]**: Enforce `static_assert(std::is_trivially_copyable_v<HardwarePowerBreakdown>)`.

---

## 3. Success Metrics

1. **Binary Size Reduction**: Further decrease release binary below 200 KB.
2. **Heap Allocation**: Zero dynamic heap allocations during `HardwareProbe` path initialization.
3. **Compile-Time**: Decreased preprocessor parsing time by eliminating heavy C++23 standard headers.
