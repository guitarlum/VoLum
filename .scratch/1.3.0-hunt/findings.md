# Findings

Format: see `spec.md`. Every entry needs a final Disposition other than `open`.

### F-00 Audio & MIDI devices dialog keeps the click captured
- Source: owner, manual test 08/10 21:52
- Severity: stuck-ui
- Repro: Settings > SIGNAL > Audio & MIDI devices..., close the dialog, click anywhere
- Expected / Actual: click acts normally / every click reopens the device dialog
- Disposition: fixed (5e4c00d7)

### F-01 MIDI recall with the editor closed loads the wrong custom capture
- Source: L1 MIDI hunter f38d7832 (high)
- Severity: wrong-behavior
- Repro: custom amp whose only capture is CAB1/channel 2; preset on PLAY slot 12; plugin editor closed; CC 102 = 12
- Expected / Actual: custom Sound loads / `_VolumApplyCustomMainCabs()` returns early without UI, loader uses stale DIRECT/ch1 (VoLumPlayRuntime.inc.cpp:310-359, VoLumSceneRig.inc.cpp:386-408,550-555)
- Disposition: fixed (merged into dev 97f858af; CI 37879465244 green)

### F-02 WinMM port renumbering clears the saved MIDI input
- Source: L1 MIDI hunter (high)
- Severity: data-loss (setting)
- Repro: select "Controller 2"; another MIDI device with a lower index appears; restart: selection becomes off and is written to settings.ini
- Expected / Actual: same controller reconnects or the saved selection is kept / port set to off and persisted (iPlug2 IPlugAPP_host.cpp:116-120,299-334,405-438)
- Disposition: fix-pending-verify (hunt-02 29ca2278 FAIL by 0436a709; rework ec6dfc54 + iPlug2 f94f4eb7; re-verify 0436a709; CI 37896570821)

### F-03 Cancel in the device dialog does not restore the previous MIDI port
- Source: L1 MIDI hunter (high)
- Severity: wrong-behavior
- Repro: port A active; pick B in the dialog; Cancel; B stays open. Also OK after a MIDI-only change restarts audio
- Disposition: fix-pending-verify (hunt-02 29ca2278 FAIL by 0436a709; rework ec6dfc54 + iPlug2 f94f4eb7; re-verify 0436a709; CI 37896570821)

### F-04 A flood of unsupported MIDI can drop the following Sound recall
- Source: L1 MIDI hunter (medium)
- Severity: glitch
- Repro: >32 notes / active sensing then PC 9 immediately; 32-entry queue overflows before VoLum filters
- Disposition: fix-pending-verify (hunt-02 29ca2278 FAIL by 0436a709; rework ec6dfc54 + iPlug2 f94f4eb7; re-verify 0436a709; CI 37896570821)

### F-05 An unplugged audio interface is replaced by the default device and persisted
- Source: L1 MIDI hunter (high); contradicts docs/user-guide.en.md:389-391
- Severity: data-loss (setting)
- Repro: save external interface; quit; unplug (default device present); launch; reconnect; relaunch: interface setting lost
- Disposition: fix-pending-verify (hunt-02 29ca2278 FAIL by 0436a709; rework ec6dfc54 + iPlug2 f94f4eb7; re-verify 0436a709; CI 37896570821)

### F-06 Both outputs routed to one physical channel drops the left side
- Source: L1 MIDI hunter (high)
- Severity: wrong-behavior
- Repro: output L and R both on channel 1 (or mono device); R overwrites L (IPlugAPP_host.cpp:967-995,1225-1234)
- Disposition: fix-pending-verify (hunt-02 29ca2278 FAIL by 0436a709; rework ec6dfc54 + iPlug2 f94f4eb7; re-verify 0436a709; CI 37896570821)

### F-07 Clamped channels / driver-granted buffer not reflected in UI and settings.ini
- Source: L1 MIDI hunter (medium)
- Severity: wrong-behavior (cosmetic)
- Disposition: fix-pending-verify (hunt-02 29ca2278 FAIL by 0436a709; rework ec6dfc54 + iPlug2 f94f4eb7; re-verify 0436a709; CI 37896570821)

### F-08 Host mode change with the editor closed overwrites that mode's saved knobs
- Source: L1 state hunter 97bca9ee (high). Pitch/Delay/Reverb/Tremolo/Chorus
- Severity: data-loss
- Repro: two Chorus modes with different knobs; close editor; automate Chorus Mode; save/reload project: new mode's snapshot = old knobs (NeuralAmpModeler.cpp:1778-1947 UI-only path; VoLumSettingsLocks.inc.cpp:91-166)
- Disposition: rework (08c6e5cf fixes Pitch/Delay/Reverb mode/Tremolo/Chorus; re-verify a6f0d11d FAIL: Oktaverb sub-mode snapshot still overwritten by a knob change before idle, stale Oktaverb requests re-applied after restore, tests drive a test-local harness; resumed 6e59de48)

### F-09 1.2.x custom-scene lazy migration can delete the only durable copy
- Source: state hunter (high)
- Severity: data-loss
- Repro: 1.2.3 library with custom scene + old project; open in 1.3.0; close DAW without saving; reopen: scene gone (VoLumSceneRig.inc.cpp:578-600 erase; ContentStore no longer writes legacy customScenes)
- Disposition: fix-pending-verify (hunt-11 265856f1 opus 021addf6: library writes legacy customScenes back unchanged, merge keeps disk copy unless this writer deleted the amp, instances copy instead of move, unreadable IR stays uncalibrated; 4 tests red 26/58 -> green; verifier bb23893d (gpt); CI 37896275378)

### F-10 An empty custom-scene map in a current-schema chunk cannot clear stale instance scenes
- Source: state hunter (high); also Everything Pack "restore machine settings"
- Severity: wrong-behavior
- Disposition: open (hunt-04 unserialize)

### F-11 Pre-1.2 chunk loads can inherit the machine-global custom amp when the UI opens
- Source: state hunter (high)
- Severity: wrong-behavior
- Disposition: open (hunt-04 unserialize)

### F-12 Standalone full settings save overwrites newer plugin machine settings (Lite, Animate, calibration)
- Source: state hunter (high)
- Severity: data-loss (settings)
- Disposition: open

### F-13 Metronome BPM/volume/time signature never persisted
- Source: state hunter (medium)
- Severity: wrong-behavior (feature gap)
- Disposition: reported (needs a state-format addition; post-1.3.0, see Q-02)

### F-14 Standalone window size not restored
- Source: state hunter. Severity: cosmetic
- Disposition: question (Q-03)

### F-15 Settings tab resets to SIGNAL when the editor reopens
- Source: state hunter. Severity: cosmetic
- Disposition: question (Q-03)

### F-16 Digital Delay Age noise depends on host block size (RNG reseeded every callback)
- Source: L1 DSP hunter 1b9c9e71 (high). AudioDSPTools/dsp/Delay.cpp:267-284
- Severity: wrong-sound
- Disposition: reported (fix moves Digital/Age golden renders)

### F-17 Analog Delay near 2000 ms wraps and reads ~9 ms behind the write head
- Source: DSP hunter (high). Delay.cpp ring = 2 s, chorus adds up to 11 ms
- Severity: wrong-sound
- Disposition: open (hunt-06 AudioDSPTools)

### F-18 Reverse Delay silently caps at 1000 ms
- Source: DSP hunter (high)
- Severity: wrong-sound
- Disposition: open (hunt-06 AudioDSPTools)

### F-19 Tuner cannot read low E at 192 kHz (max YIN lag 2047 < 2330)
- Source: DSP hunter (high). VoLumTunerDSP.h
- Severity: wrong-behavior
- Disposition: open (hunt-07 tuner/chorus)

### F-20 Reopening the tuner analyzes stale audio
- Source: DSP hunter (high)
- Severity: wrong-behavior
- Disposition: open (hunt-07 tuner/chorus)

### F-21 Pitch character / mode switch jumps the read head by hundreds of samples (click)
- Source: DSP hunter (high)
- Severity: glitch
- Disposition: reported (crossfading read heads is a DSP redesign; post-1.3.0)

### F-22 Chorus mode-switch duck length depends on block size and rate
- Source: DSP hunter (high). VoLumChorus.h:178-190,330-436
- Severity: glitch
- Disposition: open (hunt-07 tuner/chorus)

### F-23 Support-only Dual Amp (MAIN missing) is silent but still reports support latency
- Source: DSP hunter (high)
- Severity: wrong-behavior (edge)
- Disposition: open (hunt-05 reset/audio)

### F-24 Delay/Reverb/filter outputs can allocate on the audio thread when the host block grows
- Source: DSP hunter (high)
- Severity: glitch
- Disposition: open (hunt-06 AudioDSPTools)

### F-25 IR low/high cut filters keep stale state while bypassed / across IR switch and reset
- Source: DSP hunter (high). VoLumIrShapingDsp.h
- Severity: glitch
- Disposition: open (hunt-05 reset/audio)

### F-26 Delay time glide speed scales with sample rate
- Source: DSP hunter (high)
- Severity: wrong-sound
- Disposition: reported (changes automation renders; post-1.3.0)

### F-27 Selected MAIN custom amp is tracked by list index: swaps after Pack Reset or a delete in another instance
- Source: L1 preset hunter 12d8a42a (medium)
- Severity: wrong-behavior / data-loss risk
- Disposition: open (hunt-08 selection/idle)

### F-28 Importing part of an Everything Pack replaces the whole PLAY board (also under Add)
- Source: preset hunter (medium-high); contradicts guide line 371
- Severity: data-loss
- Disposition: open (hunt-09 pack/repair)

### F-29 " 2"-suffixed preset names over 28 bytes can no longer be updated with Ctrl+S
- Source: preset hunter (high). VoLumCustomContentApi.h:724-753
- Severity: wrong-behavior
- Disposition: open (hunt-10 small model fixes)

### F-30 MIDI recall while the save dialog is open saves into the wrong amp
- Source: preset hunter (medium). NeuralAmpModeler.cpp:940-952
- Severity: wrong-behavior
- Disposition: open (hunt-08 selection/idle)

### F-31 Deleting a custom amp gives no PLAY warning; presets keep dualAmpActive with an empty SUPPORT
- Source: preset hunter (high)
- Severity: wrong-behavior
- Disposition: open (hunt-09 pack/repair)

### F-32 LIVE marker jumps to the wrong row after a swap when the board has duplicates
- Source: preset hunter (high). Severity: cosmetic
- Disposition: open (hunt-10 small model fixes)

### F-33 User section of the PLAY/MIDI pickers is not sorted (factory:10 before factory:2)
- Source: preset hunter (high). Severity: cosmetic
- Disposition: open (hunt-10 small model fixes)

### F-34 Pack Reset preview does not name PLAY switches that will go Invalid
- Source: preset hunter (medium). Severity: wrong-behavior (low)
- Disposition: open (hunt-09 pack/repair)

### F-35 Save dialog duplicate check is case-sensitive, Manage is not
- Source: preset hunter (high). Severity: cosmetic
- Disposition: open (hunt-10 small model fixes)

### F-36 Guide says only the standalone Everything export carries MIDI slots; the plugin does too
- Source: preset hunter. Severity: docs (guide line 363)
- Disposition: fixed (docs merged 07bf039b)

### F-37 OnReset rebuilds live DSP (NAM, chorus, delay, reverb, scratch) while ProcessBlock uses it (AU/AAX)
- Source: L1 thread hunter fa9cbea8 (high on AU)
- Severity: crash
- Repro: Logic, dual amp + chorus + delay, change sample rate / buffer / stop while playing
- Disposition: open (hunt-05 reset/audio)

### F-38 Sample-rate change never rebuilds the support cab IR
- Source: thread hunter (high). NeuralAmpModeler.cpp:2258-2276
- Severity: wrong-sound
- Disposition: open (hunt-05 reset/audio)

### F-39 Host state restore calls _VolumRefreshChannels / _VolumSelectCustomAmp on the host thread
- Source: thread hunter (medium). Unserialization.cpp:826,856
- Severity: crash
- Disposition: open (hunt-04 unserialize)

### F-40 VST3 OnParamChange on the audio thread walks the UI, scans disk, calls SetLatency (Dual Amp, support amp, PRE pitch/NAM)
- Source: thread hunter (high). NeuralAmpModeler.cpp:1566,1574-1579,1634-1657
- Severity: crash / hang
- Disposition: fix-pending-verify (rework 08c6e5cf: F-40 kept + latency safety + immediate Dual Amp update; re-verify a6f0d11d; CI 37890075013)

### F-41 Name dialog acts on the second click of the double-click that opened it (cancels, or saves unconfirmed)
- Source: L1 modal hunter 8f62a8ee (high). VoLumNameDialog.h:184-186,275-297
- Severity: wrong-behavior
- Repro: PLAY, modified sound, double-click "+ Add this sound" (or double-click "Save current as new..." in the preset menu)
- Disposition: open (hunt-13 input)

### F-42 Double-click that closes a dropdown/overlay leaks its second click into switches, header buttons, hero
- Source: modal hunter (medium-high). NAMSwitchControl, NAMCircleButtonControl, VoLumMetronomeButtonControl, VoLumHero mDblAsSingleClick
- Severity: wrong-behavior
- Repro: tuner open (T), double-click NOISE GATE outside the panel: overlay closes and gate toggles; IR dropdown double-click over the Dual chip turns Dual Amp on
- Disposition: open (hunt-13 input)

### F-43 P with the Custom IR dropdown open leaves it over PLAY (clickable, changes the hidden cab)
- Source: modal hunter + UI hunter 943b46a8 (high). VoLumLayoutBuild.inc.cpp:1497-1500, VoLumPlayRuntime.inc.cpp:3-55
- Severity: stuck-ui
- Disposition: open (hunt-13 input)

### F-44 macOS file pickers opened on mouse-down keep the mouse grab
- Source: modal hunter (medium). VoLumPackActions.inc.cpp:72,102; VoLumCustomOverlay.h:737,1100
- Severity: wrong-behavior (macOS)
- Disposition: open (hunt-13 input)

### F-45 Non-interactive IR recall (MIDI, host restore) can pop modal error boxes and re-enter OnIdle
- Source: modal hunter (medium). VoLumSceneRig.inc.cpp:672-689
- Severity: stuck-ui
- Disposition: open (hunt-13 input)

### F-46 Library notice can open mid knob drag and leave the host gesture open
- Source: modal hunter (low-medium). NeuralAmpModeler.cpp:906-913
- Severity: wrong-behavior (minor)
- Disposition: open (hunt-13 input)

### F-47 First save after a 1.2.x upgrade drops custom-amp scenes for every amp not focused that session
- Source: L1 pack hunter 0de494ea (high); same root as F-09
- Severity: data-loss
- Disposition: fix-pending-verify (hunt-11 265856f1 opus 021addf6: library writes legacy customScenes back unchanged, merge keeps disk copy unless this writer deleted the amp, instances copy instead of move, unreadable IR stays uncalibrated; 4 tests red 26/58 -> green; verifier bb23893d (gpt); CI 37896275378)

### F-48 An unreadable IR during trim migration is marked calibrated at 0 dB forever
- Source: pack hunter (high). VoLumSceneRig.inc.cpp:916-952
- Severity: wrong-behavior
- Disposition: fix-pending-verify (hunt-11 265856f1 opus 021addf6: library writes legacy customScenes back unchanged, merge keeps disk copy unless this writer deleted the amp, instances copy instead of move, unreadable IR stays uncalibrated; 4 tests red 26/58 -> green; verifier bb23893d (gpt); CI 37896275378)

### F-49 A crash mid Pack import leaves replaced captures; the next import deletes the only rollback
- Source: pack hunter (high)
- Severity: data-loss
- Disposition: open (hunt-14 pack robustness)

### F-50 Wrong JSON type in a Pack manifest throws out of OpenPack
- Source: pack hunter (high). VoLumPack.h:509-577
- Severity: crash
- Disposition: open (hunt-14 pack robustness)

### F-51 Zip with repeated central-directory entries copies one payload until memory runs out
- Source: pack hunter (high). VoLumPackArchive.h:262-299
- Severity: crash
- Disposition: open (hunt-14 pack robustness)

### F-52 Pack import can mint custom pedal slots past 127
- Source: pack hunter (high). VoLumPack.h:1213-1250
- Severity: wrong-behavior
- Disposition: open (hunt-09 pack/repair)

### F-53 Library entry whose capture file is not in the Pack still imports
- Source: pack hunter (medium). VoLumPack.h:545-564
- Severity: wrong-behavior
- Disposition: open (hunt-09 pack/repair)

### F-54 Pack payload paths differing only by case overwrite each other on Windows/macOS
- Source: pack hunter (medium)
- Severity: data-loss (hand-built packs)
- Disposition: open (hunt-14 pack robustness)

### F-55 Pack export truncates the destination before writing; restored settings.json is not validated
- Source: pack hunter (medium). VoLumPackArchive.h:319-334, VoLumPack.h:1365-1371
- Severity: data-loss
- Disposition: open (hunt-14 pack robustness)

### F-56 1.2.3 drops midiSoundMap on save; 1.3.0 strips unknown per-item keys of a newer library
- Source: pack hunter (high)
- Severity: data-loss (downgrade / forward compat)
- Disposition: reported (format; release notes should say a 1.2.3 downgrade loses the PLAY board)

### F-57 Guide names the import backup volum-content.json.pre-import.bak; code writes .packbak
- Source: pack hunter. Severity: docs
- Disposition: fixed (docs merged 07bf039b)

### F-58 PLAY banner amp name draws through "(unsaved)" with long preset names
- Source: UI hunter (high). VoLumPlaySurface.h DrawStage
- Severity: cosmetic
- Disposition: open (hunt-15 PLAY text/cpu)

### F-59 Sound picker preset and amp columns overlap with long names
- Source: UI hunter (medium). Severity: cosmetic
- Disposition: open (hunt-15 PLAY text/cpu)

### F-60 Pack export "Sounds" amp label overwrites the preset name
- Source: UI hunter (medium). VoLumPackOverlay.h. Severity: cosmetic
- Disposition: open (hunt-15 PLAY text/cpu)

### F-61 PLAY dirties the full window every idle tick, also under Settings/tuner and with Animate art off
- Source: UI hunter (high). Severity: cpu
- Disposition: open (hunt-15: skip Tick while a full-window overlay is open; freezing the corona with Animate off is a visible change -> post-1.3.0)

### F-62 (suspected) MIDI recall in a plugin host does not update the host's parameter view
- Source: REAPER harness writer 96e8a80b (code read). VolumRecallSound uses SendParameterValueFromDelegate, which never reaches the VST3 controller copy; REAPER's generic UI / automation lanes may show old knobs until reopen.
- Severity: host display / automation
- Disposition: open (confirm in REAPER phase 2 via fact host_param_view_follows_midi_recall; candidate fix DirtyParametersFromUI() at end of VolumRecallSound)

### F-63 Standalone: Output mode Raw resets to Normalized on every relaunch
- Source: UI tester A b1787d8d (2/2). kOutputMode is a param; standalone settings JSON has no output-mode key. LITE/FULL and Animate art persist.
- Severity: wrong-behavior
- Disposition: open (hunt-16: persist in the machine-settings sidecar as an optional key; no preset/state chunk change)

### F-64 Update check: appcast 404 (Pages not enabled), message blames the connection, notes would show "## What's Changed"
- Source: UI tester A b1787d8d. Update check is new in 1.3.0; repo has_pages=false, so publish-appcast.yml's deploy step will fail at release. First non-empty body line is a markdown heading on every release.
- Severity: release risk
- Disposition: workflow notes fixed (fc843df1); Pages is owner action Q-07; message wording deferred (nit). Was: (owner action before release: Settings > Pages > Source "GitHub Actions" -> Q-07. hunt-17: workflow picks the first non-heading, non-empty line and strips markdown; message wording for unreadable vs offline if the HTTP layer can tell)

### F-65 DirectSound offers no 48 kHz on the UA-2X2 (RtAudio capture probe checks only legacy format flags)
- Source: UI tester A. iPlug2 fork RtAudio.cpp ~5774-5791. ASIO4ALL offers 48k.
- Severity: wrong-behavior (driver-specific)
- Disposition: reported (vendored RtAudio, post-1.3.0; ASIO is the recommended path; mention in docs troubleshooting)

### F-66 Resize grip can grow the standalone past the work area; grip then unreachable
- Source: UI tester A. Severity: UX
- Disposition: open (hunt-18 small UI: clamp to the monitor work area)

### F-67 Recall CC stepper wraps 119 -> 0 (channel stepper clamps); CC 0 accepted although guide says Bank Select is not supported
- Source: UI tester A. Severity: wrong-behavior/docs
- Disposition: open (hunt-18: clamp like the channel stepper; docs: CC0 can be chosen as the recall CC, bank select as banking is not supported)

### F-68 Guide :307 and changelog say Settings reopens on the last tab; only true within a session
- Source: UI tester A. Severity: docs
- Disposition: fixed (docs merged 07bf039b no longer claim it; persistence is Q-03)

### F-69 Audio & MIDI dialog centres on the screen, not over VoLum
- Source: UI tester A. Severity: cosmetic (iPlug2 APP host)
- Disposition: reported (post-1.3.0)

### F-70 Shortcut card omits P and Ctrl+,; Pack save dialog filter is a bare "*.volumpack"; recipes write the IR name with "/" (library stores "_")
- Source: UI tester A. Severity: cosmetic/docs
- Disposition: open (hunt-18 shortcut card + filter description; docs lane for recipes)

### F-72 VST3 ignores MIDI Program Change in REAPER (macOS CI, reproducible); AU and CC 102 work
- Source: mac evidence phase 2 a6e3835a, run 37873627110 (feature/hunt-mac-ci fb03dc43). VST3 PC 1: state and RMS unchanged; VST3 CC 102=2, AU PC 1, AU CC 102 all recall.
- Suspects: (a) iPlug2 fork IPlugVST3_ProcessorBase.cpp:334 decodes PC as (int)(value*127.), which truncates 1/127 in float to 0 (slot 0, possibly the current Sound -> no change); same truncation for aftertouch; (b) REAPER delivers VST3 PC only to a kIsProgramChange parameter (iPlug kPresetParam exists only with NPresets>0), not via IMidiMapping kCtrlProgramChange.
- Severity: wrong-behavior, headline 1.3.0 feature in plugin hosts
- Disposition: fix-pending-verify (hunt-19 2696f8dd + iPlug2 410255270: per-channel VST3 program lists + getUnitByBus + rounding; verifier 20ac1d0a FAIL: reload-delivered program param recalls (Cubase dummy PC); resumed 0b7b707e for an idempotent reload guard; rework 993e49a3 + iPlug2 1e0d6b85e drops kCanAutomate (verifier: spec-correct) + adds VST3ProgramRestoreGuard; re-verify 20ac1d0a FAIL: missing ProcessContext reuses stale kPlaying, Arm/EndBlock race loses a re-arm, zero-frame/suspended processing leaves the guard armed forever, tests only cover a model helper; resumed 0b7b707e again; mac evidence phase 3 a6e3835a on 993e49a3 (run 37894739681) PASS: REAPER macOS delivers VST3 PC 1 without kCanAutomate, AU + VST3 PC1/CC102/reload all recall and reopen without a `[midi] recall` line; then REAPER Windows + one mac evidence re-run on the reworked guard)

### F-73 IR import accepts non-WAV files (junk renamed .wav, 0-byte, header-only, .txt); error only when picked
- Source: UI tester B 1c820716 (5/5). VoLumCustomOverlay.h ~800-843 checks size only (VoLumIrFileGuard.h:23).
- Severity: wrong-behavior
- Disposition: open (hunt-20 IR: validate at import with the existing "File is not a WAV file." message, don't copy)

### F-74 Confirm dialog body is one clipped centred line (IR delete question cut on both sides)
- Source: UI tester B. VoLumConfirmDialog.h:73-77. Severity: UX
- Disposition: open (hunt-20: wrap the body)

### F-75 IR-delete confirm names the last factory amp ("Soldano SLO100") while MAIN is a custom amp
- Source: UI tester B; VoLumRigRepair.inc.cpp:53. Severity: wrong-behavior
- Disposition: open (hunt-20)

### F-76 Deleting the playing IR on a custom amp leaves MAIN on No Cab instead of the stock cab
- Source: UI tester B (2/2). Guide: "a deleted IR returns to the stock cab". Severity: wrong-behavior
- Disposition: open (hunt-20)

### F-77 Exact-value entry drops the decimal comma ("7,5" -> 75 -> clamps 10.0); first Esc only clears text, next click swallowed
- Source: UI tester B (2/2). iPlug2 fork IGraphicsWin.cpp ~757-799 keystroke filter. Severity: wrong-behavior (German users)
- Disposition: open (hunt-21 iPlug2 text entry: accept ',' as decimal, one Esc cancels)

### F-78 Custom amp row "Monomyth Skelet" cut without ellipsis
- Source: UI tester B. Severity: cosmetic
- Disposition: open (hunt-18)

### F-79 Preset name length cap counts bytes (umlauts/CJK get fewer chars); CJK glyphs missing in the font
- Source: UI tester B. Severity: cosmetic/UX
- Disposition: open (hunt-18: count code points; missing CJK glyphs reported, post-1.3.0)

### F-80 POST card labels touch their LED; Delay DIVISION/FEEDBACK and Oktaverb PRE-DLY/INTENSITY crowd
- Source: UI tester B. Severity: cosmetic
- Disposition: open (hunt-18 if a layout constant; else post-1.3.0)

### F-81 Keyboard-hint footer shows internal param names ("ReverbMix", "DelayTime") and goes stale after card focus
- Source: UI tester B. Severity: UX
- Disposition: open (hunt-18: display labels; clear on focus change)

### F-82 Manage overwrite icon moves the selection dot even when the overwrite is cancelled
- Source: UI tester B. Severity: UX
- Disposition: open (hunt-18)

### F-83 Guide says S / Shift+S step through Custom IR; VoLumCabStep.h deliberately cycles the four fixed slots
- Source: UI tester B. Severity: docs
- Disposition: open (docs touch-up batch)

### F-84 Test flakes under full-suite load: test_volum_realtime_budget.cpp:247 (passes alone), test_volum_pack.cpp:808 (transient)
- Source: hunt-03 worker 6e59de48. Severity: test reliability (CI risk)
- Disposition: open (verifier a6f0d11d: pre-existing. realtime budget = wall-clock limit under load; pack test uses fixed %TEMP%\volum-pack-tests\<name> shared by all worktrees -> add PID. hunt-22 test hygiene)

### F-85 scripts/check-local-guards.ps1:12 assumes .git is a directory (wrong in git worktrees)
- Source: hunt-03 worker. Severity: tooling
- Disposition: fixed (merged 362bbb13)

### F-86 iPlug2 IPlugAPP_dialog.cpp:360-365 writes into reserved-but-unresized std::string storage (UB)
- Source: hunt-02 worker b0b7ac3a. Severity: latent crash
- Disposition: confirmed by verifier 0436a709; folded into hunt-02 rework

### F-87 VST3 MIDI is channel 1 only: VST3_NUM_MIDI_IN_CHANS / VST3_NUM_CC_CHANS=16 in config.h never reach the iPlug2 VST3 sources
- Source: hunt-19 worker 0b7b707e, confirmed by verifier 20ac1d0a (pre-existing: CC 102 on channels 2-16 already broken in VST3) (compile-time probe printed 1; built plugin reports event bus channelCount=1). 'One channel = N' (N>1) and recall from other channels cannot work in VST3.
- Severity: wrong-behavior (VST3)
- Disposition: open (after hunt-19 REAPER check: define both macros in VST3 project settings (Windows props + Mac xcconfig), verify with REAPER harness; adds ~2000 host-visible MIDI params -> host check; else document 'VST3: channel 1' in the guide)

### F-88 Pack import with "Also restore machine settings" saves the old live sound over the restored amp
- Source: UI tester C a9f7d217 (2/2: Reset and Overwrite with the box). Restored scene equals the outgoing THC Sunset scene field for field; Dual partner dropped; persists after relaunch. VoLumPackActions.inc.cpp:212-218 already comments on this failure (9dc4500d); suspect the debounced settings save (19be2b67).
- Severity: data-loss
- Disposition: open (hunt-23, top priority; coordinate with settings unit 60ec32a7 F-12)

### F-89 Sidebar keeps the old custom-amp name and art after an import replaces that amp (same id)
- Source: UI tester C (2/2). Severity: wrong-behavior
- Disposition: open (hunt-09 pack/repair)

### F-90 Each Tab onto a custom SUPPORT lane reloads its capture from disk
- Source: UI tester C (10/10). Factory SUPPORT and custom MAIN don't reload. Possible audio glitch (unheard).
- Severity: wrong-behavior
- Disposition: open (hunt-08 selection/idle)

### F-91 BUILD footer shows the internal custom capture filename (amp_..._import_..._AMP-MJVM-5.nam) instead of the stored original `file`
- Source: UI testers B and C. Severity: cosmetic
- Disposition: open (hunt-18)

### F-92 Custom amp: after visiting a cabless-only channel, channel 1 stays on No Cab (plays the cabless import, not the G12 capture)
- Source: UI tester C (p2-004). Severity: wrong-behavior (unconfirmed 1/1)
- Disposition: open (hunt-08 selection/idle: confirm and fix)

### F-93 _ReportedLatencySamples counts SUPPORT whenever mSupportModel is loaded; ProcessBlock also requires mVolumSupportSelected
- Source: hunt-05 worker a24a992c. Severity: wrong latency report (loaded but unselected SUPPORT)
- Disposition: confirmed by verifier f37a0a68; folded into hunt-05 rework

### F-94 _UpdateLatency / _VolumRefreshLatencyReport touch UI and host from OnReset (AAX runs OnReset on the audio thread)
- Source: hunt-05 worker. Severity: realtime (AAX only)
- Disposition: open (check whether VoLum ships AAX; if not, reported only)

### F-95 (likely harness) Standalone exits without a window for ~93 s after a 15-min monkey run (31 failed launches, then OK)
- Source: monkey runner 724038e9, seed 130201 then 130301 (`C:\dev\VoLum-wtmonkey\.monkey-runs\custom-play-130301`). No dump, no log error; the 977-event run that followed was clean.
- Likely cause: the single-instance guard (iPlug2 IPlugAPP_main.cpp:55-97) refuses to start while another VoLum holds the mutex: the previous Kill()ed process still tearing down its audio driver, or another agent's VoLum window (FindWindow matches any "VoLum"). Start-VoLum closes the "still running" dialog without logging its title.
- Severity: harness / environment unless it reproduces after a normal close
- Disposition: reported (harness follow-up: wait for every VoLum process to exit before relaunching, log startup dialog titles; L5 27d02eef: 0/12 refusals after a normal close, old process gone in 0.26-0.52 s -> harness Kill() teardown, not product)

### F-71 (unconfirmed) Import preview listed 7 replacements, volum.log said "3 replaced"
- Source: UI tester A (seen once). Severity: unknown
- Disposition: open (hunt-14 pack robustness: check preview vs apply counting)

### F-96 Two e2e scenarios read custom scenes from volum-content.json customScenes, but 1.3.0 keeps them in volum-settings.json (checks pass without testing anything)
- Source: hunt-11 worker 021addf6 (NeuralAmpModeler/scripts/e2e-standalone-win.ps1; exact lines from verifier bb23893d). Severity: test gap
- Disposition: open (fix with a GUI e2e run after hunt-11 lands)

### F-97 Dual Amp on/off clicks every time (Support lane switched in with no crossfade)
- Source: L5 audio lane 27d02eef (real loopback, DirectSound 512, Marshall 2204 + THC Sunset SUPPORT, 200 Hz tone; keys 2 then Space). 6/6: one-sample step 10-15x the surrounding level; tone control clean. Evidence .scratch/1.3.0-hunt/l5/out/scan-ds512/isolated.txt. NeuralAmpModeler.cpp Dual decision follows the param directly.
- Severity: glitch
- Disposition: open (hunt-24: short equal-power crossfade on Dual toggle; guarded: golden renders must stay bit-identical; report-only if they move)

### F-98 (low confidence) Smaller steps on COMP / POST pedal toggles and sidebar amp switches
- Source: L5. COMP 3/4, POST pedal 2/4, amp switch 4/8; steps 3-7x surrounding level, control clean. Measured under heavy build load.
- Severity: glitch (low confidence)
- Disposition: reported (re-measure with tools/glitchscan when the machine is idle; fold into hunt-24 if the Dual crossfade pattern applies)

### F-99 DirectSound on the UA-2X2 offers no 48 kHz; switching from ASIO@48k shows a notice pointing to the disabled ASIO Device Settings button
- Source: L5 4/4. Preferences lists 11025/22050/44100/96000 under DirectSound; 48000 opens at 44100. Evidence l5/out/switch/applies.csv, l5/out/sanity-512/volum.log.
- Severity: UX (rate list comes from the driver probe; the notice text is ours)
- Disposition: open (small: notice wording should depend on the driver type; queue with hunt-13 or a standalone-host follow-up after hunt-02 lands)

### F-100 DirectSound drops or repeats blocks during steady play (quiet phase jumps)
- Source: L5 2/2 runs (3.6-4.2 s; ~40 s, ~75 s), DirectSound 512 under heavy build load; control clean.
- Severity: glitch (DirectSound only, likely load)
- Disposition: reported (re-check on an idle machine; ASIO is the recommended driver)
