# Phone performance pass — 2026-10-26

Samsung SM-S938U1 (Adreno830), existing Balanced75%/60 configuration.
Baseline retained locally in build/verification/performance-baseline.txt (ignored).
16 presentation windows with >30,000 D3D calls/frame: minimum57.6, median59.35,
maximum60.0FPS; median sampled graphics-bridge cost3.725ms/frame. These are
race-like windows selected by call count, not a controlled replay or sustained
thermal test. Current Android thermal status was0 (none); that does not establish
thermal stability over a long race. Presets are unchanged pending measurements.

The dev build now logs completed-presentation interval p50/p95/p99/max every120
frames, plus count exceeding1.5 times the selected frame budget (25ms at60FPS).
A bounded120-double array uses no per-frame allocation. Statistics include loading,
pauses and scene transitions; separate those from active race samples when analyzing.
These are CPU-side completed Present intervals, not GPU timestamps or proof of
actual display scanout. Existing bridge/present/hand-over statistics remain.

GameActivity logs Android thermal status, battery temperature and charging source
every10seconds while resumed. Sampling stops on pause and has no overlay or auto
quality changes. Battery temperature is not a CPU/GPU temperature. Charging and
Device-panel streaming can affect power/heat, so note both when comparing runs.

Validation: host frame_metrics test checks quantiles, bounded storage, reset,
invalid samples and uncapped behavior. Android build/signature/install and runtime
log verification are recorded in HANDOFF. For sustained validation, drive a normal
race for ~5minutes, then pull logcat and compare early/late active-race intervals
and thermal state before changing resolution or FPS presets.

## Sustained phone run

User completed the requested normal-race test. PID29828 capture10:20:21–10:26:19
is retained locally as build/verification/performance-sustained.txt. Excluding
startup/menu windows using >30k calls and the latest valid vehicle snapshot with
forward speed magnitude>3m/s gives116 windows (13,920 intervals), spanning
10:21:38–10:25:51. This heuristic can include transitions within a120-frame window;
it is not a deterministic replay or a GPU/display timing trace.

- Median window FPS59.6; lowest window56.9.
- Median window p95 interval17.265ms; median window p99 interval18.79ms.
  These are medians of window quantiles, not whole-run quantiles.
-11/13,920 intervals exceeded25ms (~0.079%); maximum41.51ms.
- Early versus late thirds: median FPS59.6/59.6, median p95 17.265/17.22ms,
  median p99 18.835/18.8ms. Late third has7 late intervals versus2 early,
  but no sustained frame-time degradation.
- All37 thermal samples report status0, unplugged. Battery32.1→33.3°C.
  After-run HAL skin36.8°C, AP43.5°C, battery33.8°C, thermal status0.

Decision: retain Balanced75%/60FPS and current buffer count. This run supplies no
basis for reducing quality or adding automatic thermal changes. Occasional brief
hitches remain; investigate a reproducible hitch before changing core scheduling.
No evidence here establishes performance on other phones, longer runs, or higher
quality settings.
