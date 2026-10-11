# REF-ARCH-088: Hardware-Verified Fan Curve Feedback & Visual Actuation Topology

## 1. Subsystem Architecture

```
+-------------------------------------------------------------------------+
|                  Qt6 / QML FanCurveStudioView                           |
|                                                                         |
|  [Header]:  Profile Selector  |  Custom Badge  |  [🟢 EC L5 (4333 RPM)] |
|                                                                         |
|  [Canvas]:  +---------------------------------------------------------+ |
|             |  Interactive Spline Canvas                              | |
|             |  --> Cyber Pulse Glow Animation (Opacity 0.35 -> 0.0)   | |
|             +---------------------------------------------------------+ |
|                                                                         |
|  [Action]:  +---------------------------------------------------------+ |
|             | 💾 [커브 적용] -> backend.applyFanCurves()               | |
|             | 📢 "✅ [데몬 동기화 성공] ThinkPad EC L5 즉시 구동"      | |
|             +---------------------------------------------------------+ |
+------------------------------------+------------------------------------+
                                     | 1. "SET_FAN_CURVES\n" + data
                                     | 2. "OK LEVEL=5 RPM=4333 TEMP=54.2 PROFILE=1"
                                     v
+-------------------------------------------------------------------------+
|                       WattCurb Root Daemon                              |
|                                                                         |
|  1. peer_is_authorized_client() authentication                          |
|  2. deserialize_curves() & save_to_file("/etc/wattcurb/fan_curves.conf")|
|  3. apply_fan_for_temp() -> Writes /proc/acpi/ibm/fan                   |
|  4. Read back live hardware state: read(/proc/acpi/ibm/fan)             |
|  5. Format & return structured telemetry ACK                            |
+-------------------------------------------------------------------------+
```

---

## 2. Wire Format Specification

* **Request**:
  `SET_FAN_CURVES\nPROFILE_0_POINTS=...`
* **Response Format**:
  `OK LEVEL=<int> RPM=<uint> TEMP=<float> PROFILE=<int>\n`
  * Example: `OK LEVEL=5 RPM=4333 TEMP=54.2 PROFILE=1\n`
  * Full-Speed Example: `OK LEVEL=8 RPM=5420 TEMP=71.5 PROFILE=0\n`
* **Error Response Format**:
  `ERROR: <reason>\n`
