# REF-REQ-095: Watt-Reactive Procedural Tray Icon Specification

## 1. Context
The tray served static freedesktop icon names
(`battery-070-profile-performance`), so the badge was whatever the system theme
shipped and carried no power information. `IconPixmap` was an empty stub.
Continuous, watt-driven colour requires the icon to be drawn.

## 2. Functional Requirements

### REQ-095.1: One badge shape per profile
| Profile | Badge | Rationale |
| :--- | :--- | :--- |
| Performance | Rocket | thrust / unrestricted |
| Balanced | Analog gauge with needle | regulated, measurable |
| PowerSaver | Leaf | conservation |
| UltraEndurance | **Snowflake** | deep freeze; radial straight-line geometry is the opposite silhouette to the leaf, so the two saving profiles can never be confused |

### REQ-095.2: Badge colour tracks power draw
The badge is filled with a continuous green -> lime -> amber -> red ramp driven
by system watts. The red channel must never dip across the ramp.

### REQ-095.3: Colour is normalised inside each profile's own band
`t` in [0, 1] is computed against a per-profile band, not one global scale, so
the saving profiles retain a usable colour spread instead of sitting permanently
green:

| Profile | green (t=0) | red (t=1) |
| :--- | ---: | ---: |
| Performance | 8 W | 35 W |
| Balanced | 6 W | 24 W |
| PowerSaver | 4 W | 16 W |
| UltraEndurance | 3 W | 10 W |

The Performance band is this platform's measured p05..p95 over a 7-day history
(5,473 samples: p05 8.0 W, median 14.5 W, p95 34.2 W). Values outside a band
clamp. Bands must tighten monotonically towards the saving profiles.

### REQ-095.4: The Balanced gauge deflects
The needle sweeps from 180 degrees (left, green) to 0 degrees (right, red) in
proportion to `t`, and the arc fills behind it. The deflection must be
measurable, not decorative.

### REQ-095.5: Charging state
While charging, the battery frame is drawn in bright blue (`#38BDF8`), at any
charge level.

### REQ-095.6: Low-battery warning
When not charging, the frame is white at 30% and above, and fades white -> red as
the charge falls from 30% to 0%. The ramp must be monotonic.

### REQ-095.7: The glyph owns the tile
No element may enclose or inset the glyph. Battery charge, charging state and the
low warning are carried by a slim rail along the bottom edge. The glyph and the
rail must both remain legible at 22 px, the size Plasma uses in a default panel.

## 3. Non-Functional Requirements
- Rendering is allocation-free, uses no external graphics dependency, and is
  served over SNI `IconPixmap` (ARGB32, network byte order) at 22, 32 and 48 px.
- `IconName` is deliberately empty; a non-empty name takes precedence over
  `IconPixmap` in Plasma.
- A render must complete in `< 4 ms/op` at 48 px, and repeat property reads must
  be served from cache.

## 4. Verification
[`REF-TEST-058`](file:///home/jedclub/Develop/WattCurb/tests/test_units.cpp):
band ordering and clamping, ramp monotonicity, frame colour rules, four distinct
silhouettes, measured needle deflection, and the render Oracle Gate.
