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
- Disposition: fixed (feature/hunt-02 merged into dev 74d006df; cold-verified, CI green)

### F-03 Cancel in the device dialog does not restore the previous MIDI port
- Source: L1 MIDI hunter (high)
- Severity: wrong-behavior
- Repro: port A active; pick B in the dialog; Cancel; B stays open. Also OK after a MIDI-only change restarts audio
- Disposition: fixed (feature/hunt-02 merged into dev 74d006df; cold-verified, CI green)

### F-04 A flood of unsupported MIDI can drop the following Sound recall
- Source: L1 MIDI hunter (medium)
- Severity: glitch
- Repro: >32 notes / active sensing then PC 9 immediately; 32-entry queue overflows before VoLum filters
- Disposition: fixed (feature/hunt-02 merged into dev 74d006df; cold-verified, CI green)

### F-05 An unplugged audio interface is replaced by the default device and persisted
- Source: L1 MIDI hunter (high); contradicts docs/user-guide.en.md:389-391
- Severity: data-loss (setting)
- Repro: save external interface; quit; unplug (default device present); launch; reconnect; relaunch: interface setting lost
- Disposition: fixed (feature/hunt-02 merged into dev 74d006df; cold-verified, CI green)

### F-06 Both outputs routed to one physical channel drops the left side
- Source: L1 MIDI hunter (high)
- Severity: wrong-behavior
- Repro: output L and R both on channel 1 (or mono device); R overwrites L (IPlugAPP_host.cpp:967-995,1225-1234)
- Disposition: fixed (feature/hunt-02 merged into dev 74d006df; cold-verified, CI green)

### F-07 Clamped channels / driver-granted buffer not reflected in UI and settings.ini
- Source: L1 MIDI hunter (medium)
- Severity: wrong-behavior (cosmetic)
- Disposition: fixed (feature/hunt-02 merged into dev 74d006df; cold-verified, CI green)

### F-08 Host mode change with the editor closed overwrites that mode's saved knobs
- Source: L1 state hunter 97bca9ee (high). Pitch/Delay/Reverb/Tremolo/Chorus
- Severity: data-loss
- Repro: two Chorus modes with different knobs; close editor; automate Chorus Mode; save/reload project: new mode's snapshot = old knobs (NeuralAmpModeler.cpp:1778-1947 UI-only path; VoLumSettingsLocks.inc.cpp:91-166)
- Disposition: fixed (feature/hunt-03 merged into dev 8806c6a0; cold-verified, CI green)

### F-09 1.2.x custom-scene lazy migration can delete the only durable copy
- Source: state hunter (high)
- Severity: data-loss
- Repro: 1.2.3 library with custom scene + old project; open in 1.3.0; close DAW without saving; reopen: scene gone (VoLumSceneRig.inc.cpp:578-600 erase; ContentStore no longer writes legacy customScenes)
- Disposition: fixed (feature/hunt-11 merged into dev 223a9739; cold-verified, CI green)

### F-10 An empty custom-scene map in a current-schema chunk cannot clear stale instance scenes
- Source: state hunter (high); also Everything Pack "restore machine settings"
- Severity: wrong-behavior
- Disposition: fixed (feature/hunt-04 merged into dev 998b6e2d; cold-verified, CI green)

### F-11 Pre-1.2 chunk loads can inherit the machine-global custom amp when the UI opens
- Source: state hunter (high)
- Severity: wrong-behavior
- Disposition: fixed (feature/hunt-04 merged into dev 998b6e2d; cold-verified, CI green)

### F-12 Standalone full settings save overwrites newer plugin machine settings (Lite, Animate, calibration)
- Source: state hunter (high)
- Severity: data-loss (settings)
- Disposition: fixed (feature/hunt-12 merged into dev 4532ddd2; cold-verified, CI green)

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
- Disposition: fixed (feature/hunt-07 merged into dev a4f856de; cold-verified, CI green)

### F-18 Reverse Delay silently caps at 1000 ms
- Source: DSP hunter (high)
- Severity: wrong-sound
- Disposition: fixed (feature/hunt-07 merged into dev a4f856de; cold-verified, CI green)

### F-19 Tuner cannot read low E at 192 kHz (max YIN lag 2047 < 2330)
- Source: DSP hunter (high). VoLumTunerDSP.h
- Severity: wrong-behavior
- Disposition: fixed (feature/hunt-07 merged into dev a4f856de; cold-verified, CI green)

### F-20 Reopening the tuner analyzes stale audio
- Source: DSP hunter (high)
- Severity: wrong-behavior
- Disposition: fixed (feature/hunt-07 merged into dev a4f856de; cold-verified, CI green)

### F-21 Pitch character / mode switch jumps the read head by hundreds of samples (click)
- Source: DSP hunter (high)
- Severity: glitch
- Disposition: reported (crossfading read heads is a DSP redesign; post-1.3.0)

### F-22 Chorus mode-switch duck length depends on block size and rate
- Source: DSP hunter (high). VoLumChorus.h:178-190,330-436
- Severity: glitch
- Disposition: fixed (feature/hunt-07 merged into dev a4f856de; cold-verified, CI green)

### F-23 Support-only Dual Amp (MAIN missing) is silent but still reports support latency
- Source: DSP hunter (high)
- Severity: wrong-behavior (edge)
- Disposition: fixed (feature/hunt-05 merged into dev b352ce1b; cold-verified, CI green)

### F-24 Delay/Reverb/filter outputs can allocate on the audio thread when the host block grows
- Source: DSP hunter (high)
- Severity: glitch
- Disposition: fixed (feature/hunt-07 merged into dev a4f856de; cold-verified, CI green)

### F-25 IR low/high cut filters keep stale state while bypassed / across IR switch and reset
- Source: DSP hunter (high). VoLumIrShapingDsp.h
- Severity: glitch
- Disposition: fixed (feature/hunt-05 merged into dev b352ce1b; cold-verified, CI green)

### F-26 Delay time glide speed scales with sample rate
- Source: DSP hunter (high)
- Severity: wrong-sound
- Disposition: reported (changes automation renders; post-1.3.0)

### F-27 Selected MAIN custom amp is tracked by list index: swaps after Pack Reset or a delete in another instance
- Source: L1 preset hunter 12d8a42a (medium)
- Severity: wrong-behavior / data-loss risk
- Disposition: fixed (feature/hunt-08 merged into dev eed6e090; cold-verified, CI green)

### F-28 Importing part of an Everything Pack replaces the whole PLAY board (also under Add)
- Source: preset hunter (medium-high); contradicts guide line 371
- Severity: data-loss
- Disposition: fixed (feature/hunt-09 merged into dev bab7ee01; cold-verified, CI green)

### F-29 " 2"-suffixed preset names over 28 bytes can no longer be updated with Ctrl+S
- Source: preset hunter (high). VoLumCustomContentApi.h:724-753
- Severity: wrong-behavior
- Disposition: fixed (feature/hunt-08 merged into dev eed6e090; cold-verified, CI green)

### F-30 MIDI recall while the save dialog is open saves into the wrong amp
- Source: preset hunter (medium). NeuralAmpModeler.cpp:940-952
- Severity: wrong-behavior
- Disposition: fixed (feature/hunt-08 merged into dev eed6e090; cold-verified, CI green)

### F-31 Deleting a custom amp gives no PLAY warning; presets keep dualAmpActive with an empty SUPPORT
- Source: preset hunter (high)
- Severity: wrong-behavior
- Disposition: fixed (feature/hunt-09 merged into dev bab7ee01; cold-verified, CI green)

### F-32 LIVE marker jumps to the wrong row after a swap when the board has duplicates
- Source: preset hunter (high). Severity: cosmetic
- Disposition: fixed (feature/hunt-08 merged into dev eed6e090; cold-verified, CI green)

### F-33 User section of the PLAY/MIDI pickers is not sorted (factory:10 before factory:2)
- Source: preset hunter (high). Severity: cosmetic
- Disposition: fixed (feature/hunt-08 merged into dev eed6e090; cold-verified, CI green)

### F-34 Pack Reset preview does not name PLAY switches that will go Invalid
- Source: preset hunter (medium). Severity: wrong-behavior (low)
- Disposition: deferred to 1.3.1 (trim, 09/10 12:00)

### F-35 Save dialog duplicate check is case-sensitive, Manage is not
- Source: preset hunter (high). Severity: cosmetic
- Disposition: fixed (feature/hunt-08 merged into dev eed6e090; cold-verified, CI green)

### F-36 Guide says only the standalone Everything export carries MIDI slots; the plugin does too
- Source: preset hunter. Severity: docs (guide line 363)
- Disposition: fixed (docs merged 07bf039b)

### F-37 OnReset rebuilds live DSP (NAM, chorus, delay, reverb, scratch) while ProcessBlock uses it (AU/AAX)
- Source: L1 thread hunter fa9cbea8 (high on AU)
- Severity: crash
- Repro: Logic, dual amp + chorus + delay, change sample rate / buffer / stop while playing
- Disposition: fixed (feature/hunt-05 merged into dev b352ce1b; cold-verified, CI green)

### F-38 Sample-rate change never rebuilds the support cab IR
- Source: thread hunter (high). NeuralAmpModeler.cpp:2258-2276
- Severity: wrong-sound
- Disposition: fixed (feature/hunt-05 merged into dev b352ce1b; cold-verified, CI green)

### F-39 Host state restore calls _VolumRefreshChannels / _VolumSelectCustomAmp on the host thread
- Source: thread hunter (medium). Unserialization.cpp:826,856
- Severity: crash
- Disposition: fixed (feature/hunt-04 merged into dev 998b6e2d; cold-verified, CI green)

### F-40 VST3 OnParamChange on the audio thread walks the UI, scans disk, calls SetLatency (Dual Amp, support amp, PRE pitch/NAM)
- Source: thread hunter (high). NeuralAmpModeler.cpp:1566,1574-1579,1634-1657
- Severity: crash / hang
- Disposition: fixed (feature/hunt-03 merged into dev 8806c6a0; cold-verified, CI green)

### F-41 Name dialog acts on the second click of the double-click that opened it (cancels, or saves unconfirmed)
- Source: L1 modal hunter 8f62a8ee (high). VoLumNameDialog.h:184-186,275-297
- Severity: wrong-behavior
- Repro: PLAY, modified sound, double-click "+ Add this sound" (or double-click "Save current as new..." in the preset menu)
- Disposition: fixed (feature/hunt-13 merged into dev 50720379; cold-verified, CI green)

### F-42 Double-click that closes a dropdown/overlay leaks its second click into switches, header buttons, hero
- Source: modal hunter (medium-high). NAMSwitchControl, NAMCircleButtonControl, VoLumMetronomeButtonControl, VoLumHero mDblAsSingleClick
- Severity: wrong-behavior
- Repro: tuner open (T), double-click NOISE GATE outside the panel: overlay closes and gate toggles; IR dropdown double-click over the Dual chip turns Dual Amp on
- Disposition: fixed (feature/hunt-13 merged into dev 50720379; cold-verified, CI green)

### F-43 P with the Custom IR dropdown open leaves it over PLAY (clickable, changes the hidden cab)
- Source: modal hunter + UI hunter 943b46a8 (high). VoLumLayoutBuild.inc.cpp:1497-1500, VoLumPlayRuntime.inc.cpp:3-55
- Severity: stuck-ui
- Disposition: fixed (feature/hunt-13 merged into dev 50720379; cold-verified, CI green)

### F-44 macOS file pickers opened on mouse-down keep the mouse grab
- Source: modal hunter (medium). VoLumPackActions.inc.cpp:72,102; VoLumCustomOverlay.h:737,1100
- Severity: wrong-behavior (macOS)
- Disposition: fixed (feature/hunt-13 merged into dev 50720379; cold-verified, CI green)

### F-45 Non-interactive IR recall (MIDI, host restore) can pop modal error boxes and re-enter OnIdle
- Source: modal hunter (medium). VoLumSceneRig.inc.cpp:672-689
- Severity: stuck-ui
- Disposition: fixed (feature/hunt-13 merged into dev 50720379; cold-verified, CI green)

### F-46 Library notice can open mid knob drag and leave the host gesture open
- Source: modal hunter (low-medium). NeuralAmpModeler.cpp:906-913
- Severity: wrong-behavior (minor)
- Disposition: fixed (feature/hunt-13 merged into dev 50720379; cold-verified, CI green)

### F-47 First save after a 1.2.x upgrade drops custom-amp scenes for every amp not focused that session
- Source: L1 pack hunter 0de494ea (high); same root as F-09
- Severity: data-loss
- Disposition: fixed (feature/hunt-11 merged into dev 223a9739; cold-verified, CI green)

### F-48 An unreadable IR during trim migration is marked calibrated at 0 dB forever
- Source: pack hunter (high). VoLumSceneRig.inc.cpp:916-952
- Severity: wrong-behavior
- Disposition: fixed (feature/hunt-11 merged into dev 223a9739; cold-verified, CI green)

### F-49 A crash mid Pack import leaves replaced captures; the next import deletes the only rollback
- Source: pack hunter (high)
- Severity: data-loss
- Disposition: fixed (feature/hunt-14 merged into dev 919b0832; cold-verified, CI green)

### F-50 Wrong JSON type in a Pack manifest throws out of OpenPack
- Source: pack hunter (high). VoLumPack.h:509-577
- Severity: crash
- Disposition: fixed (feature/hunt-14 merged into dev 919b0832; cold-verified, CI green)

### F-51 Zip with repeated central-directory entries copies one payload until memory runs out
- Source: pack hunter (high). VoLumPackArchive.h:262-299
- Severity: crash
- Disposition: fixed (feature/hunt-14 merged into dev 919b0832; cold-verified, CI green)

### F-52 Pack import can mint custom pedal slots past 127
- Source: pack hunter (high). VoLumPack.h:1213-1250
- Severity: wrong-behavior
- Disposition: fixed (feature/hunt-09 merged into dev bab7ee01; cold-verified, CI green)

### F-53 Library entry whose capture file is not in the Pack still imports
- Source: pack hunter (medium). VoLumPack.h:545-564
- Severity: wrong-behavior
- Disposition: fixed (feature/hunt-09 merged into dev bab7ee01; cold-verified, CI green)

### F-54 Pack payload paths differing only by case overwrite each other on Windows/macOS
- Source: pack hunter (medium)
- Severity: data-loss (hand-built packs)
- Disposition: fixed (feature/hunt-14 merged into dev 919b0832; cold-verified, CI green)

### F-55 Pack export truncates the destination before writing; restored settings.json is not validated
- Source: pack hunter (medium). VoLumPackArchive.h:319-334, VoLumPack.h:1365-1371
- Severity: data-loss
- Disposition: fixed (feature/hunt-14 merged into dev 919b0832; cold-verified, CI green)

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
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-59 Sound picker preset and amp columns overlap with long names
- Source: UI hunter (medium). Severity: cosmetic
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-60 Pack export "Sounds" amp label overwrites the preset name
- Source: UI hunter (medium). VoLumPackOverlay.h. Severity: cosmetic
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-61 PLAY dirties the full window every idle tick, also under Settings/tuner and with Animate art off
- Source: UI hunter (high). Severity: cpu
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-62 (suspected) MIDI recall in a plugin host does not update the host's parameter view
- Source: REAPER harness writer 96e8a80b (code read). VolumRecallSound uses SendParameterValueFromDelegate, which never reaches the VST3 controller copy; REAPER's generic UI / automation lanes may show old knobs until reopen.
- Severity: host display / automation
- Disposition: deferred to 1.3.1 (confirmed on Windows REAPER b34418a9; host param view stays stale after MIDI recall, state/audio correct)

### F-63 Standalone: Output mode Raw resets to Normalized on every relaunch
- Source: UI tester A b1787d8d (2/2). kOutputMode is a param; standalone settings JSON has no output-mode key. LITE/FULL and Animate art persist.
- Severity: wrong-behavior
- Disposition: fixed (feature/hunt-12 merged into dev 4532ddd2; cold-verified, CI green)

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
- Disposition: fixed (feature/hunt-18 merged into dev 5642ea4a; cold-verified, CI green)

### F-67 Recall CC stepper wraps 119 -> 0 (channel stepper clamps); CC 0 accepted although guide says Bank Select is not supported
- Source: UI tester A. Severity: wrong-behavior/docs
- Disposition: fixed (feature/hunt-18 merged into dev 5642ea4a; cold-verified, CI green)

### F-68 Guide :307 and changelog say Settings reopens on the last tab; only true within a session
- Source: UI tester A. Severity: docs
- Disposition: fixed (docs merged 07bf039b no longer claim it; persistence is Q-03)

### F-69 Audio & MIDI dialog centres on the screen, not over VoLum
- Source: UI tester A. Severity: cosmetic (iPlug2 APP host)
- Disposition: reported (post-1.3.0)

### F-70 Shortcut card omits P and Ctrl+,; Pack save dialog filter is a bare "*.volumpack"; recipes write the IR name with "/" (library stores "_")
- Source: UI tester A. Severity: cosmetic/docs
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-72 VST3 ignores MIDI Program Change in REAPER (macOS CI, reproducible); AU and CC 102 work
- Source: mac evidence phase 2 a6e3835a, run 37873627110 (feature/hunt-mac-ci fb03dc43). VST3 PC 1: state and RMS unchanged; VST3 CC 102=2, AU PC 1, AU CC 102 all recall.
- Suspects: (a) iPlug2 fork IPlugVST3_ProcessorBase.cpp:334 decodes PC as (int)(value*127.), which truncates 1/127 in float to 0 (slot 0, possibly the current Sound -> no change); same truncation for aftertouch; (b) REAPER delivers VST3 PC only to a kIsProgramChange parameter (iPlug kPresetParam exists only with NPresets>0), not via IMidiMapping kCtrlProgramChange.
- Severity: wrong-behavior, headline 1.3.0 feature in plugin hosts
- Disposition: merged into dev d271fd49 (hunt-19; Mac phase 4 + Windows REAPER phase 2 restore guard PASS; Q-16 first-block limitation waived; CI 37918559753 green; iPlug2 fork volum/1.3.0-integration 723b1f3f)

### F-73 IR import accepts non-WAV files (junk renamed .wav, 0-byte, header-only, .txt); error only when picked
- Source: UI tester B 1c820716 (5/5). VoLumCustomOverlay.h ~800-843 checks size only (VoLumIrFileGuard.h:23).
- Severity: wrong-behavior
- Disposition: fixed (feature/hunt-20 merged into dev 2f982022; cold-verified, CI green)

### F-74 Confirm dialog body is one clipped centred line (IR delete question cut on both sides)
- Source: UI tester B. VoLumConfirmDialog.h:73-77. Severity: UX
- Disposition: fixed (feature/hunt-20 merged into dev 2f982022; cold-verified, CI green)

### F-75 IR-delete confirm names the last factory amp ("Soldano SLO100") while MAIN is a custom amp
- Source: UI tester B; VoLumRigRepair.inc.cpp:53. Severity: wrong-behavior
- Disposition: fixed (feature/hunt-20 merged into dev 2f982022; cold-verified, CI green)

### F-76 Deleting the playing IR on a custom amp leaves MAIN on No Cab instead of the stock cab
- Source: UI tester B (2/2). Guide: "a deleted IR returns to the stock cab". Severity: wrong-behavior
- Disposition: fixed (feature/hunt-20 merged into dev 2f982022; cold-verified, CI green)

### F-77 Exact-value entry drops the decimal comma ("7,5" -> 75 -> clamps 10.0); first Esc only clears text, next click swallowed
- Source: UI tester B (2/2). iPlug2 fork IGraphicsWin.cpp ~757-799 keystroke filter. Severity: wrong-behavior (German users)
- Disposition: fixed (feature/hunt-18 merged into dev 5642ea4a; cold-verified, CI green)

### F-78 Custom amp row "Monomyth Skelet" cut without ellipsis
- Source: UI tester B. Severity: cosmetic
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-79 Preset name length cap counts bytes (umlauts/CJK get fewer chars); CJK glyphs missing in the font
- Source: UI tester B. Severity: cosmetic/UX
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-80 POST card labels touch their LED; Delay DIVISION/FEEDBACK and Oktaverb PRE-DLY/INTENSITY crowd
- Source: UI tester B. Severity: cosmetic
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-81 Keyboard-hint footer shows internal param names ("ReverbMix", "DelayTime") and goes stale after card focus
- Source: UI tester B. Severity: UX
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-82 Manage overwrite icon moves the selection dot even when the overwrite is cancelled
- Source: UI tester B. Severity: UX
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-83 Guide says S / Shift+S step through Custom IR; VoLumCabStep.h deliberately cycles the four fixed slots
- Source: UI tester B. Severity: docs
- Disposition: fixed (feature/hunt-18 merged into dev 5642ea4a; cold-verified, CI green)

### F-84 Test flakes under full-suite load: test_volum_realtime_budget.cpp:247 (passes alone), test_volum_pack.cpp:808 (transient)
- Source: hunt-03 worker 6e59de48. Severity: test reliability (CI risk)
- Disposition: fixed (feature/hunt-22 merged into dev 62a652fd; cold-verified, CI green)

### F-85 scripts/check-local-guards.ps1:12 assumes .git is a directory (wrong in git worktrees)
- Source: hunt-03 worker. Severity: tooling
- Disposition: fixed (merged 362bbb13)

### F-86 iPlug2 IPlugAPP_dialog.cpp:360-365 writes into reserved-but-unresized std::string storage (UB)
- Source: hunt-02 worker b0b7ac3a. Severity: latent crash
- Disposition: fixed (feature/hunt-02 merged into dev 74d006df; cold-verified, CI green)

### F-87 VST3 MIDI is channel 1 only: VST3_NUM_MIDI_IN_CHANS / VST3_NUM_CC_CHANS=16 in config.h never reach the iPlug2 VST3 sources
- Source: hunt-19 worker 0b7b707e, confirmed by verifier 20ac1d0a (pre-existing: CC 102 on channels 2-16 already broken in VST3) (compile-time probe printed 1; built plugin reports event bus channelCount=1). 'One channel = N' (N>1) and recall from other channels cannot work in VST3.
- Severity: wrong-behavior (VST3)
- Disposition: fixed for 1.3.0 as docs ("VST3: MIDI channel 1", feature/hunt-18 merged 5642ea4a); 16-channel wiring deferred to 1.3.1

### F-88 Pack import with "Also restore machine settings" saves the old live sound over the restored amp
- Source: UI tester C a9f7d217 (2/2: Reset and Overwrite with the box). Restored scene equals the outgoing THC Sunset scene field for field; Dual partner dropped; persists after relaunch. VoLumPackActions.inc.cpp:212-218 already comments on this failure (9dc4500d); suspect the debounced settings save (19be2b67).
- Severity: data-loss
- Disposition: merged into dev 603dfd40 (hunt-23; e2e red 6/21 -> green 21/21; CI 37908391618 green)

### F-89 Sidebar keeps the old custom-amp name and art after an import replaces that amp (same id)
- Source: UI tester C (2/2). Severity: wrong-behavior
- Disposition: fixed (feature/hunt-09 merged into dev bab7ee01; cold-verified, CI green)

### F-90 Each Tab onto a custom SUPPORT lane reloads its capture from disk
- Source: UI tester C (10/10). Factory SUPPORT and custom MAIN don't reload. Possible audio glitch (unheard).
- Severity: wrong-behavior
- Disposition: fixed (feature/hunt-08 merged into dev eed6e090; cold-verified, CI green)

### F-91 BUILD footer shows the internal custom capture filename (amp_..._import_..._AMP-MJVM-5.nam) instead of the stored original `file`
- Source: UI testers B and C. Severity: cosmetic
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-92 Custom amp: after visiting a cabless-only channel, channel 1 stays on No Cab (plays the cabless import, not the G12 capture)
- Source: UI tester C (p2-004). Severity: wrong-behavior (unconfirmed 1/1)
- Disposition: not a bug (hunt-08 565bd9b9: SnapSlotForChannel keeps the current cab by design, pinned by a test); UX question Q-17.2

### F-93 _ReportedLatencySamples counts SUPPORT whenever mSupportModel is loaded; ProcessBlock also requires mVolumSupportSelected
- Source: hunt-05 worker a24a992c. Severity: wrong latency report (loaded but unselected SUPPORT)
- Disposition: fixed (feature/hunt-05 merged into dev b352ce1b; cold-verified, CI green)

### F-94 _UpdateLatency / _VolumRefreshLatencyReport touch UI and host from OnReset (AAX runs OnReset on the audio thread)
- Source: hunt-05 worker. Severity: realtime (AAX only)
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-95 (likely harness) Standalone exits without a window for ~93 s after a 15-min monkey run (31 failed launches, then OK)
- Source: monkey runner 724038e9, seed 130201 then 130301 (`.scratch/1.3.0-hunt/evidence/custom-play-130301`). No dump, no log error; the 977-event run that followed was clean.
- Likely cause: the single-instance guard (iPlug2 IPlugAPP_main.cpp:55-97) refuses to start while another VoLum holds the mutex: the previous Kill()ed process still tearing down its audio driver, or another agent's VoLum window (FindWindow matches any "VoLum"). Start-VoLum closes the "still running" dialog without logging its title.
- Severity: harness / environment unless it reproduces after a normal close
- Disposition: reported (harness follow-up: wait for every VoLum process to exit before relaunching, log startup dialog titles; L5 27d02eef: 0/12 refusals after a normal close, old process gone in 0.26-0.52 s -> harness: 27e7830a confirmed the old classifier closed VoLum's own main #32770 window during startup, which quit the app)

### F-71 (unconfirmed) Import preview listed 7 replacements, volum.log said "3 replaced"
- Source: UI tester A (seen once). Severity: unknown
- Disposition: fixed (feature/hunt-14 merged into dev 919b0832; cold-verified, CI green)

### F-96 Two e2e scenarios read custom scenes from volum-content.json customScenes, but 1.3.0 keeps them in volum-settings.json (checks pass without testing anything)
- Source: hunt-11 worker 021addf6 (NeuralAmpModeler/scripts/e2e-standalone-win.ps1; exact lines from verifier bb23893d). Severity: test gap
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-97 Dual Amp on/off clicks every time (Support lane switched in with no crossfade)
- Source: L5 audio lane 27d02eef (real loopback, DirectSound 512, Marshall 2204 + THC Sunset SUPPORT, 200 Hz tone; keys 2 then Space). 6/6: one-sample step 10-15x the surrounding level; tone control clean. Evidence .scratch/1.3.0-hunt/l5/out/scan-ds512/isolated.txt. NeuralAmpModeler.cpp Dual decision follows the param directly.
- Severity: glitch
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-98 (low confidence) Smaller steps on COMP / POST pedal toggles and sidebar amp switches
- Source: L5. COMP 3/4, POST pedal 2/4, amp switch 4/8; steps 3-7x surrounding level, control clean. Measured under heavy build load.
- Severity: glitch (low confidence)
- Disposition: reported (re-measure with tools/glitchscan when the machine is idle; fold into hunt-24 if the Dual crossfade pattern applies)

### F-99 DirectSound on the UA-2X2 offers no 48 kHz; switching from ASIO@48k shows a notice pointing to the disabled ASIO Device Settings button
- Source: L5 4/4. Preferences lists 11025/22050/44100/96000 under DirectSound; 48000 opens at 44100. Evidence l5/out/switch/applies.csv, l5/out/sanity-512/volum.log.
- Severity: UX (rate list comes from the driver probe; the notice text is ours)
- Disposition: fixed (feature/hunt-18 merged into dev 5642ea4a; cold-verified, CI green)

### F-100 DirectSound drops or repeats blocks during steady play (quiet phase jumps)
- Source: L5 2/2 runs (3.6-4.2 s; ~40 s, ~75 s), DirectSound 512 under heavy build load; control clean.
- Severity: glitch (DirectSound only, likely load)
- Disposition: deferred to 1.3.1 (user trim 12:00: polish/cosmetic/harness/guarded)

### F-101 Standalone editor captured as a blank grey surface (only a meter drawn) after random keyboard/mouse input
- Source: monkey top-up 724038e9: blank PrintWindow frames 11/11 (keyboard 130401), 9/10 (mouse 130402), 2/10 (overlay 130201), 0/11 (PLAY 130301). Fresh launch of the same dev exe renders fine with PrintWindow and CopyFromScreen (coordinator check 09:45).
- Severity: harness artefact (not a product bug)
- Disposition: reported, harness (724038e9 retry 10:05: with classifier 27e7830a the exact actions 0-99 of seed 130401 render correctly; P alone renders 3/3; earlier blank frames came from the old harness closing/relaunching the main window)

### F-102 Pack restore writes the Dual sidecar after the library commit (should-fix, from hunt-23 verify)
`VoLumPackActions.inc.cpp:224-233`: the sidecar write fails or runs after the library and main settings are already committed, so a crash in between leaves mixed disk state. Disposition: queued into hunt-09 (pack/repair), together with the hunt-14 atomic ApplyPack settings write.

### F-103 Pack machine-settings test uses a fixed temp directory (should-fix, from hunt-23 verify)
`test_volum_pack_machine_settings.cpp:282` can collide when worktrees run tests in parallel. Disposition: queued into hunt-09.

Note for the hunt-12 merge: the Pack settings replacement must take hunt-12's settings-file lock.

### F-104 IR content equality ignores trimCalibrated; hunt-11 e2e/integration gaps (should-fix, from hunt-11 verify)
- `VoLumContentStore.h:1162`: a readable zero-energy IR can stay "uncalibrated" forever. `test_volum_upgrade_migration.cpp:338-364` models the plugin lifecycle locally; no pin that `_VolumCustomScene` uses the production helper.
- Disposition: deferred to 1.3.1 (edge case; user trim)

### F-105 Deleting the IR SUPPORT is playing leaves SUPPORT on No Cab (from hunt-20 verify)
- `VoLumRigRepair.inc.cpp:117` still calls `_VolumClearIR(true, true)`; guide and dialog promise a baked-cab fallback. Pre-existing.
- Disposition: fixed (feature/hunt-20 merged into dev 2f982022; cold-verified, CI green)

### F-106 Pack settings whitelist has no test tying it to the writer's keys; quarantine folders never cleaned (should-fix, from hunt-14 verify)
- `VoLumPack.h:1128-1135` repeats the settings writer's 20 keys; a new key would silently drop from restores. `.volumpack-quarantine-*` folders are never removed.
- Disposition: deferred to 1.3.1 (trim)

### F-107 Nested mode apply can re-apply an older Oktaverb/Reverb value (should-fix, from hunt-03 verify)
- `_VolumApplyPendingModeChanges` (NeuralAmpModeler.cpp ~1577) notifies via SendParameterValueFromDelegate -> OnParamChangeUI -> applies again (line ~1878); if audio-thread changes arrive mid-apply the outer call can apply its older read. Fix: re-entrancy flag.
- Disposition: deferred to 1.3.1 (rare race; user trim)

### F-108 REAPER harness RMS tripwires are not meaningful (harness)
- core (default vs dry, tremolo, pitch), midi-cc102 audio delta, two-instances slot-2 RMS, restore-guard-fx-bypass audio delta, offline-vs-realtime (realtime path records through the IN1/OUT1 loopback cable). RMS cannot see pitch/tremolo and saturated amps share RMS; recall itself is proven by the volum.log slot lines and state hashes.
- Disposition: deferred to 1.3.1 (harness: use spectral/correlation metrics)

### F-109 Offline render right after setState can use the previous NAM models (pre-existing)
- MAIN/SUPPORT/PRE model reloads requested by a host restore are serviced by OnIdle (NeuralAmpModeler.cpp ~990-1080), so a render started before the next idle tick still plays the old captures. Found by hunt-04 verify 3ac86549.
- Disposition: deferred to 1.3.1 (architecture; not introduced in 1.3.0)

### F-110 Host knob write just before or during a Delay/Reverb mode change is also saved into the outgoing mode's snapshot (deferred to 1.3.1)

From hunt-03 verify round 5 (4cdd2f3e) at bd5c82dc: NeuralAmpModeler.cpp:1802-1827 and VoLumSettingsLocks.inc.cpp:353-365. The save paths ignore late or current host-knob marks, so a host write that lands just before or during the mode-change transaction ends up in the old mode's snapshot. The live value and the new mode are correct. Dev with the editor open has the same behavior (the mode-switch recall saves the live knobs into the old mode), so this is not a regression. Waived for 1.3.0 under the trim. Fix: the save of the outgoing snapshot also skips knobs that are IsMarked.

### F-111 Standalone aborts when a DirectSound stream error happens mid-session (pre-existing, upstream iPlug2)
- Two crash dumps 09/10 22:14 (wt25 build, e2e upgrade scenario while the REAPER harness held the audio device): RtApiDs::callbackEvent -> RtApi::error throws RtAudioError on the RtAudio callback thread, uncaught -> terminate/abort (0xC0000409). IPlugAPP_host.cpp:1237 passes the error callback commented out (`/*, &ErrorCallback */`, same as upstream), and IPlugAPPHost::ErrorCallback is an empty TODO, so RtAudio throws instead of aborting the stream. Same behaviour in 1.2.x; needs another app to grab or lose the device. L5 (445 s DirectSound scan, 25 driver switches) never hit it.
- Fix (iPlug2 fork): pass &ErrorCallback to openStream (RtAudio then aborts the stream and reports instead of throwing) and make ErrorCallback log the error and post a 'audio stopped, reopen Preferences' notice.
- Disposition: deferred to 1.3.1 (pre-existing, trim); top of the 1.3.1 list.

### F-112 AU accepts a rejected state chunk as restored (pre-existing, upstream iPlug2)
- IPlugAU.cpp:1502 tests `!UnserializeState(chunk, 0)`, but VoLum's readers return -1 for a truncated or rejected chunk (Unserialization.cpp ~532, ~626, ~932), so the AU reports noErr and runs OnRestoreState on an instance that applied nothing. The instance keeps its constructor state, which since 895eff72 means PLAY like any new insert, instead of the BUILD a legacy project would get. The project's sound is lost either way. Found by the GPT cold verify of the PLAY-default branch. Also: the restore-path tests pin source text because no test can instantiate the plugin.
- Fix (iPlug2 fork): `if (UnserializeState(chunk, 0) < 0)`; check the VST3 and CLAP setState paths for the same pattern.
- Disposition: deferred to 1.3.1 (pre-existing, corrupt chunks only).

### F-113 macOS standalone fails to open audio on the first launch over a 1.2.x settings.ini
- Source: 1.3.0 CI installer upgrade, run 38198542144 (job 114655847979), both v1.2.3 and v1.2.2 legs. Overnight summary: `.scratch/1.3.0-ci-e2e/morning.md`.
- Severity: wrong-behavior
- Repro: install 1.2.3 or 1.2.2, let that standalone write `settings.ini` (`driver=0`, `indev`/`outdev` = Apple Virtual Sound Device, `in1=1`, `out1=1`, `out2=2`, `sr=48000`, `buffer=512`), then launch 1.3.0 against the same `HOME`.
- Expected / Actual: 1.3.0 opens the stream the way 1.2.x just did on that runner / `volum.log` says `RtApiCore::probeDeviceOpen: the device (0) does not support the requested channel count`, then `audio failure: Audio device failed to open. Reverting to the previous working settings.` A modal Audio Error is answered OK. The window still appears (PLAY, five Factory Sounds) and the process quits with no crash report. "device (0)" looks like an index lookup instead of the named device. Suspect `IPlugAPP_host.cpp` device lookup (iPlug2 commits since v1.2.3 include `4a9af5d04`, `f94f4eb7f`, `8376f6e7c`, `05eca3e58`); not confirmed.
- Disposition: deferred to 1.3.1. Not a crash, hang, stuck UI, or confirmed library data loss, so 1.3.0 CI records it as a warning and does not fail the installer job.
