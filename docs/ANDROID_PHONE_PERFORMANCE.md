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
