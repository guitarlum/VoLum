# Refactor and performance opportunities

Format per entry: `### O-NN title`, file(s), what, why, class `safe` (bit-identical golden
+ state format, no UI behavior change) or `post-1.3.0`, status `open | merged (commit) |
on post-1.3.0-opt (commit) | dropped (why)`.

### O-01 Filter unsupported MIDI before the APP queue
- Files: iPlug2/IPlug/APP/IPlugAPP_host.cpp. Class: safe. Status: open (folded into hunt-02 F-04)

### O-02 Noise gate listener copies a vector<vector<double>> every block
- Files: AudioDSPTools/dsp/NoiseGate.h:40-43, NoiseGate.cpp:102-105. Class: safe (borrowed view, bit-identical). Status: open (hunt-06)

### O-03 Analog Delay feedback-filter/tone coefficients recomputed per sample
- Files: AudioDSPTools/dsp/Delay.cpp:390-399. Class: safe if bit-identical (hoist exact same expression). Status: open (hunt-06)

### O-04 Dual-amp output peak scan walks the buffer again after the merge
- Files: NeuralAmpModeler.cpp:764-770. Class: safe. Status: open (hunt-05 if trivial)

### O-05 _VolumTogglePlayBypass searches params by name although kPlayBypassParams exists
- Class: safe. Status: open (hunt-10)

### O-06 VolumRecallSound calls _VolumSaveCurrentToSettings twice on the custom path
- Class: safe. Status: open (hunt-08)

### O-07 Memoize PLAY banner MeasureText (DirectWrite every frame)
- Files: VoLumPlaySurface.h DrawStage. Class: safe. Status: open (hunt-15)

### O-08 CRC32 table as constexpr
- Files: VoLumPackArchive.h:40-58. Class: safe. Status: open (hunt-14)

### O-09 One shared dropdown tag list (the copies caused F-43)
- Files: VoLumKeyboard.inc.cpp, VoLumLayoutBuild.inc.cpp, VoLumAmpMenus.inc.cpp. Class: safe. Status: open (hunt-13)

### O-10 Split VoLumContentStore.h (~2200 lines) and VoLumPack.h (~1380) into codec vs file IO
- Class: post-1.3.0 (pure move, but large diff right before release). Status: open

### O-11 Content-store "unchanged" checks dump the whole library to JSON twice per flush
- Class: post-1.3.0. Status: open

### O-12 _VolumRefreshPlaySurface rebuilds slots/choices every idle tick
- Class: post-1.3.0. Status: open

### O-13 Split AppState into audio and MIDI states; stable MIDI endpoint records; drop unused MIDI out
- Class: post-1.3.0. Status: open

### O-14 Shared smoothing policy and Reset() contract for stateful DSP
- Class: post-1.3.0. Status: open

### O-15 Shared hide-on-down + swallow-double-click helper for overlays
- Class: post-1.3.0. Status: open

### O-16 Delete unused NAMFileBrowserControl
- Files: NeuralAmpModelerControls.h:511-555. Class: post-1.3.0. Status: open

### O-17 Unify preset-identity restoration and dual-amp JSON readers
- Class: safe per hunter, but touches restore order. Treat as post-1.3.0. Status: open
