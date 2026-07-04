# VoLum Android PoC — build, install & on-device validation

Proof-of-concept Android host for VoLum. It reuses the **shared C++ DSP core**
(NAMCore + AudioDSPTools + VoLum `ToneStack` / `Delay` / `Reverb` / `Tremolo`)
behind an **Oboe** low-latency duplex audio host, with a **native Jetpack
Compose** UI, targeting a **USB audio interface** on a Samsung S20.

## What is built and verified

| Area | State | Verified how |
|------|-------|--------------|
| **Toolchain + NDK** | ✅ | `libvolum.so` builds for `arm64-v8a` + `x86_64`. |
| **DSP core smoke** | ✅ | Instrumented `NamSmokeTest` loads a bundled `.nam`, processes 4000 blocks, **finite output** — passes on emulator. |
| **Real-time engine (Oboe)** | ✅ live on emulator | `oboe::FullDuplexStream` duplex (mono in → stereo out) runs `VolumMobileEngine`; the app goes **Live** with `latency`/`xruns`/`peak` reported. |
| **Full signal chain** | ✅ deterministic test | `nativeEngineSelfTest` = **PASS (9 groups)**: bypass identity, unity passthrough, output-gain law, gate finiteness, model load/finite/non-silent, reverb decay tail, delay repeat, tremolo modulation, A4=440 Hz tuner detection. |
| **Tuner** | ✅ deterministic test | NSDF pitch detector (`TunerDsp.h`) fed from raw pre-gain input; UI panel shows note name + cents needle. Locks A4 within a few cents in the self-test. |
| **Native Compose UI** | ✅ screenshot-verified | Boutique dark UI: transport + meter, tuner, device/model pickers, amp knobs, and Gate / Delay / Reverb / Tremolo pedals — drives the engine live. |
| **USB device + model pick** | ✅ | `AudioDevices` lists inputs (USB first); the picker passes the chosen id to `nativeAudioStart`. 6 factory amps bundled + picker. |
| **IGraphicsAndroid backend** | ⛔ not built | Superseded for the PoC by the native Compose UI (see "UI approach"). |

### Signal chain (faithful desktop subset)

```
input gain
  → noise gate     (dsp::noise_gate::Trigger + Gain)
  → NAM model      (nam::DSP)
  → tone stack     (dsp::tone_stack::BasicNamToneStack)
  → DC blocker     (recursive_linear_filter::HighPass)
  → [mono → stereo]
  → Delay          (dsp::effect::Delay        — Digital / Analog / Reverse)
  → Reverb         (dsp::effect::Reverb       — Hall / Plate / Oktaverb)
  → Tremolo        (volum::TremoloDSP         — Optical / Bias / Harmonic, runs last)
  → output gain → final-bus safety scrub
```

These are the **actual desktop DSP classes**, not reimplementations. All
internal buffers (including every Delay/Reverb mode) are prewarmed off the audio
thread in `prepare()`, so the audio callback is allocation-free. Remaining
desktop-parity items (PRE pedals, dual-amp support lane, per-lane IR) are not
wired yet; the architecture has room for them.

### Bugs found and fixed via the local emulator

- **Dead duplex callback:** `FullDuplexStream` never pumped because the output
  stream had no data callback (`setDataCallback(this)` was missing) — the engine
  processed zero frames. Fixed; the callback now fires (`frames>0`, latency
  reported).
- **ANR on Start:** `nativeAudioStart` (stream open + NAM prewarm) ran on the UI
  thread and blocked >5 s. Moved audio start/stop/model-load off the UI thread.

## Build

```powershell
$env:JAVA_HOME = "C:\Program Files\Microsoft\jdk-17.0.19.10-hotspot"
$env:ANDROID_HOME = "C:\Android\sdk"
cd NeuralAmpModeler\projects\android
.\gradlew.bat assembleDebug     # dev/emulator
.\gradlew.bat assembleRelease   # -O3 native lib, debug-signed so it installs
```

Outputs:
- Debug APK: `app/build/outputs/apk/debug/app-debug.apk`
- Release APK: `app/build/outputs/apk/release/app-release.apk` (debug-signed)

## Tests

```powershell
# Instrumented, on a connected device/emulator (runs NamSmokeTest + engine self-test):
.\gradlew.bat connectedDebugAndroidTest
```

Both pass on the x86_64 emulator. The engine self-test is device-independent
(pure synthetic buffers), so it is the authoritative DSP-correctness gate.

## Install on the S20

```powershell
C:\Android\sdk\platform-tools\adb.exe install -r app\build\outputs\apk\release\app-release.apk
```

Grant the mic permission when prompted (duplex capture needs `RECORD_AUDIO`).

## Performance note

The emulator CPU is **x86 (WHPX-accelerated), not ARM**, so its numbers do not
represent the S20. For the record, the debug `-O0` build measured **0.19× RT**
on the emulator; the release `-O3` arm64 number on the S20 is the real gate and
must be measured on-device.

## On-device validation checklist (your handoff)

The agent cannot test live USB audio or real ARM latency — the S20 and the
interface are not attached to this machine, and an emulator cannot use a USB
audio-class interface. Please run:

1. **USB interface bring-up.** Connect the interface via USB-C OTG. Open **Input
   device** and confirm the interface appears first (tagged `USB …`). Select it.
2. **Passthrough.** Tap the **power** button, then **BYPASS ON** — you should
   hear your guitar dry through the interface. Watch the header `LATENCY … · … xr`.
3. **Tone through a NAM.** Pick an **Amp profile**, tap **LOAD AMP**, BYPASS off.
   You should hear the amp; sweep **Drive / Bass / Mid / Treble / Level**.
4. **Effects.** Toggle **Noise gate**, **Delay**, **Reverb**, **Tremolo** on and
   sweep their knobs / switch modes; confirm they sound right and stay stable.
5. **Tuner.** Toggle **Tuner ON** and play open strings; confirm the note name and
   cents needle track and settle (the tuner reads the raw input pre-gain).
6. **Stability & headroom.** Play a few minutes; `xr` (xruns) should stay low and
   the output meter should not pin red (the final-bus safety clip engages ~+3 dBFS).

Report back: latency + xruns at the interface's native buffer size, and whether
any model is too heavy for real-time. If a model can't keep up, we cap model
size / raise the buffer for the PoC.

## Recommended USB audio interface

- **Primary: Audient EVO 4** — USB-C, class-compliant (no vendor drivers, which
  Android requires), bus-powered/low draw, Hi-Z instrument input, low latency.
- **Alternatives:** Focusrite Scarlett Solo 4th Gen, MOTU M2 (both may need a
  **powered USB-C hub** due to higher bus-power draw).
- **Buy alongside:** a **USB-C OTG cable** and, as insurance, a **powered USB-C hub**.

## UI approach (why Compose, not IGraphics — for now)

The original plan reused iPlug2 `IGraphics` for the UI via a new
`IGraphicsAndroid` GLES/NanoVG backend. That backend is greenfield (upstream
iPlug2 ships no Android platform: GLES `SurfaceView`, NanoVG draw loop,
touch→mouse, timers, DPI, text entry, popups) and is only debuggable with a live
GL context — not something that can be built **and verified** in an autonomous
pass.

Per the "iterate the UI via screenshots" directive, the PoC ships a **native
Jetpack Compose** UI instead: a dark boutique-amp design (VoLum brand fonts —
Michroma display + Josefin Sans text — a warm-amber/teal/violet/green palette,
custom Canvas rotary knobs, per-pedal accents). It is fully screenshot-verified
on the emulator and drives the exact same C++ engine. If/when the IGraphics
backend is built, it can replace this screen without touching the engine.
