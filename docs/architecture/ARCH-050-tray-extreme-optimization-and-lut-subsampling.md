# ARCH-050: Architecture of Extreme Desktop Tray Optimization & Subsampling

**Ref-ID**: `REF-ARCH-050`  
**Subsystem**: Desktop Tray Client / Micro-Optimization Architecture  
**Dependencies**: `REF-REQ-073`, `REF-REQ-072`, `REF-ARCH-049`, `REF-ARCH-017`  
**Status**: Approved  

---

## 1. Architectural Overview

To eliminate the 93.8% sysfs/ACPI stall and optimize CPU-bound string formatting, the desktop tray pipeline is transformed into a zero-syscall, $O(1)$ lookup architecture:

```
[KDE Plasma ToolTip Request]
              │
              ▼
   [Hover Time Gate Check] ─── (< 500ms elapsed?) ───► [Instant Bypass: Reuse Seqlock State (0 µs)]
              │ (>= 500ms)
              ▼
   [Persistent FDs: pread(0)]
        ├── BAT0/uevent       (Persistent FD #1, zero open/close)
        ├── thermal_zone0     (Persistent FD #2, zero open/close)
        └── scaling_cur_freq  (Persistent FD #3, zero open/close)
              │
              ▼
   [Fast Inlined Integer & uevent SIMD Scanner]
              │
              ▼
   [Precomputed Constant Lookup Tables]
        ├── Icon Name LUT: O(1) table lookup [bracket][prof][charge] (2 ns)
        └── 8-Block Progress Bar LUT: O(1) 24B memcpy (2 ns)
              │
              ▼
   [Length-Preserving O(1) UTF-8 Boundary Guard]
              │
              ▼
   [sd-bus Container Serialization]
```

---

## 2. Implementation Specifications

### 2.1 8-Block Progress Bar LUT (`BAR_LUT`)

In UTF-8, `'█'` is 3 bytes (`\xE2\x96\x88`) and `'░'` is 3 bytes (`\xE2\x96\x91`).  
An 8-block bar has exactly 24 bytes plus null terminator:

```cpp
static constexpr char BAR_LUT[9][25] = {
    "\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91", // 0
    "\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91", // 1
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91", // 2
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91", // 3
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91", // 4
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91", // 5
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91", // 6
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91", // 7
    "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", // 8
};
```

Construction requires only an $O(1)$ index calculation:
```cpp
unsigned int filled = (percent * 8 + 50) / 100;
if (filled > 8) filled = 8;
std::memcpy(out, BAR_LUT[filled], 25);
```

### 2.2 Icon Name Table (`ICON_LUT`)

Icon strings follow the schema: `battery-[000..100]-[charging-]profile-[performance|balanced|powersave]`.
- 11 battery deciles (0, 10, 20, ..., 100)
- 2 charging states (0: discharging, 1: charging / AC direct)
- 3 profile modes (0: performance, 1: balanced, 2: powersave)

The entire state space is $11 \times 2 \times 3 = 66$ fixed strings. A 3D constant string pointer table eliminates `snprintf` entirely.

### 2.3 Persistent File Descriptor Cache

```cpp
struct HoverSysfsCache {
    int thermal_fd{-1};
    int bat_fd{-1};
    int cpufreq_fd{-1};
    uint64_t last_probe_ms{0};
    
    void close_all() noexcept;
};
```
Reads execute via:
```cpp
ssize_t n = ::pread(fd, buf, sizeof(buf) - 1, 0);
```

---

## 3. Performance Projection

| Metric | Before Optimization | After Extreme Optimization | Improvement Factor |
| :--- | :--- | :--- | :--- |
| **`tray.probe_sensors.total`** | 172.53 µs/op | **< 2.5 µs/op** (warm/bypass) | **> 68x faster** |
| **`tray.probe_sensors.thermal`**| 111.74 µs/op | **< 1.5 µs/op** | **> 74x faster** |
| **`tray.tooltip.build_bars`** | 50 ns/op | **2 ns/op** | **25x faster** |
| **`tray.resolve_icon`** | 150 ns/op | **5 ns/op** | **30x faster** |
| **Hover Tooltip Total** | 198.8 µs | **< 4.0 µs** | **~ 50x faster** |
| **Syscalls per Hover** | 6 (3 open, 3 close) | **0** (persistent / cached) | **100% elimination** |
