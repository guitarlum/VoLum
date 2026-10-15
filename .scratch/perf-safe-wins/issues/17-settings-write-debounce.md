# 17 Debounced standalone settings writes

Status: resolved
Blocked by: 16
Report: B1 (bg-01, bg-02). In the standalone every param change makes the next idle tick rewrite
`volum-settings.json`, the dual-amp file and the content library with write-through, and re-read the library:
during a knob drag that is ~50 writes/s on the UI thread (estimated 5-20 ms each).

## Owner decision (locked)
Write after **500 ms** without a change, and at least every **2 s** during a continuous drag. Close / quit
flushes stay synchronous.

## Paths
- `NeuralAmpModeler/NeuralAmpModeler.cpp` (`OnParamChange` dirty ~L1557-1558; OnIdle drain ~L1030-1041;
  `OnUIClose` flush ~L1388-1392; destructor ~L573-575; calibration dirty ~L1669-1670)
- `NeuralAmpModeler/VoLumSettingsScene.inc.cpp` (`_VolumSaveSettingsToFile` ~L358-436, calibration
  defaults ~L438-474)
- `NeuralAmpModeler/VoLumContentStore.h` (`Save()` ~L1582-1640)
- Pack export path (`VoLumPackActions.inc.cpp`): flush pending settings before it reads files from disk
- `NeuralAmpModeler/tests/test_volum_ui_regressions.cpp` (~L983-997 source lock on the coalesced write)
- Pure debounce helper + test (e.g. `VoLumWriteDebounce.h` + `test_volum_write_debounce.cpp`)

## Change
- A pure, clock-injected debounce helper: `dirty(now)`, `shouldWrite(now)` implementing 500 ms quiet /
  2 s max latency. OnIdle asks it before `_VolumSaveSettingsToFile`; same for the calibration defaults.
- Calibration defaults are **not** flushed on close/quit today: add that flush before debouncing them.
- Flush any pending settings before Pack export.
- `GlobalContentStore().Save()` from the settings write returns early when there is nothing to flush,
  decided by comparison (registry vs baseline + no pending deletes + registry readable), never by a dirty
  flag (`legacyCustomScenes` mutates without a Save call).

## Acceptance
- Debounce helper tests: single change writes once after 500 ms; continuous changes write at most every 2 s;
  flush writes immediately.
- Process Monitor or a write counter: a 5 s knob drag writes the settings files ~3 times instead of ~250;
  content library not rewritten when unchanged.
- Close and quit still write everything, including calibration; Pack export sees current settings.
- Updated source lock describes the new rule.

## Proof
Helper tests red against a naive "write every tick" implementation; a calibration-flush-on-close test red
against the old `OnUIClose`.

## Verifier conditions
No persistence path loses data except a hard crash inside the 500 ms / 2 s window (accepted by the owner);
atomic write-through rename unchanged; plugin formats unaffected except the calibration flush.
