---
name: VoLum Full Rig Catalog
overview: VoLum NAM Player -- 14 bundled amps with gallery UI, speaker/channel selection, comic-style amp artwork, shipped via Windows installer for standalone and VST3.
todos:
  - id: ui-mockup
    content: HTML/CSS mockup of the two-view UI (amp gallery + amp detail/controls) for layout iteration before C++ implementation
    status: pending
  - id: catalog-header
    content: Create VoLumCatalog.h with AmpInfo/ChannelInfo structs for all 14 amps (scaffold with placeholder labels for user to fill in)
    status: pending
  - id: params-hierarchy
    content: Replace single kVoLumAmpeteRig with kVoLumAmp + kVoLumSpeaker + kVoLumChannel params; update OnParamChange, rig loading, unserialization (v0.7.15)
    status: pending
  - id: paths-all-rigs
    content: "Update VoLumPaths.h: FindRigsDirectory() returns rigs/ root; load logic appends folder/speaker/channel from catalog"
    status: pending
  - id: ui-cpp
    content: Implement the finalized UI layout in C++/iPlug2 (two views, amp gallery, detail controls, channel knob)
    status: pending
  - id: installer-all-rigs
    content: Update .iss to bundle all 14 amp folders + update registry key to rigs/ root
    status: pending
  - id: images-integration
    content: Integrate comic-style amp PNGs (transparent bg) into gallery and detail view background
    status: pending
  - id: branding
    content: "Replace title/branding: VoLum logo, window title, about text"
    status: pending
  - id: readme-docs
    content: Update README and user-facing docs (install instructions, amp list, usage) once UI and installer are finalized
    status: pending
isProject: false
---

# VoLum: Full Rig Catalog + Hierarchical UI

**Product name:** VoLum

## Design decisions (confirmed)

- **Amp images:** comic/illustrated style, bold outlines, stylized, transparent background (floats on dark UI). Generated via Gemini Imagen 3. One PNG per amp (~1024x1024 source, resized in code).
- **Catalog:** hardcoded in C++ header. User provides channel labels and descriptions per amp.
- **UI framework:** iPlug2 IGraphics + NanoVG (C++, not HTML). HTML mockup for layout iteration only.

## Rig inventory (14 amps under `rigs/`)

- Ampete One -- 4 channels (1-4)
- Bad Cat mini Cat -- 3 channels (1-3)
- Brunetti XL 2 -- 3 channels (1-3)
- Fryette Deliverance 120 -- 2 channels (3,4)
- H&K TriAmp Mk2 -- 6 channels (1-6)
- Lichtlaerm Prometheus -- 3 channels (1-3)
- Marshall 2204 1982 -- 6 channels (1-6)
- Marshall JMP 2203 1976 -- 6 channels (1-4, f=FatBee, x=FatBee+Clone)
- Marshall JVM 210H OD1 -- 6 channels (1-6)
- Orange OD120 1975 -- 5 channels (1-4, f)
- Orange ORS100 1972 -- 2 channels (1,2)
- Sebago Texas Flood -- 2 channels (1,2)
- Soldano SLO100 -- 3 channels (1-3)
- THC Sunset -- 5 channels (1-5)

Each amp x 4 speaker modes (AMP/G12/G65/V30) x channels = one `.nam` file. Total ~224 files.

---

## UI concept: two views

### View 1: Amp Gallery

Full-window grid of amp thumbnails (comic illustrations) with amp names. User clicks one to select it and transitions to View 2. Accessible any time via a "back to gallery" button from View 2.

### View 2: Amp Detail + Controls

- **Hero area:** selected amp's comic illustration visible (background or prominent panel)
- **Speaker mode:** 4 buttons -- AMP (direct) / G12 / G65 / V30
- **Channel knob:** rotary knob or stepped selector cycling through 1-6 channels (varies per amp)
- **Standard NAM controls:** Input, Threshold (noise gate), Bass, Mid, Treble, Output knobs
- **Toggles:** Noise Gate on/off, EQ on/off
- **Settings gear:** opens existing preferences/calibration/output-mode panel

Window size may increase from 600x400 to ~900x600 to give the gallery and hero image room.

---

## Phase 1: Catalog data model

Replace [VoLumAmpeteCatalog.h](NeuralAmpModeler/VoLumAmpeteCatalog.h) with **`VoLumCatalog.h`**.

```cpp
struct ChannelInfo {
  const char* suffix;       // filename suffix: "1", "2", "f", "x"
  const char* label;        // UI label: "Ch 1", "FatBee", "FatBee+Clone"
  const char* description;  // tooltip: "Clean channel, low gain"
};

struct AmpInfo {
  const char* id;           // filename ID: "Ampt", "BadC", "2204"
  const char* folderName;   // disk folder: "Ampete One"
  const char* displayName;  // UI label: "Ampete One"
  const char* imageFile;    // "ampete_one.png"
  int channelCount;
  ChannelInfo channels[8];
};
```

- Speaker modes are global constants (AMP, G12, G65, V30).
- File path: `{rigsDir}/{folderName}/{speakerMode}-{id}-{suffix}.nam`

**EParams changes:**
- `kVoLumAmp` (enum 0..13), `kVoLumSpeaker` (enum 0..3), `kVoLumChannel` (enum 0..max)
- Unserialization: new version branch v0.7.15

---

## Phase 2: UI implementation

1. HTML/CSS mockup for layout iteration (Option B from earlier discussion)
2. Translate finalized layout to C++/iPlug2 IGraphics controls
3. Two-view system: gallery overlay vs detail view (show/hide control groups)
4. Channel knob: `IVKnobControl` with stepped values matching `channelCount`

---

## Phase 3: Installer + path resolution

- [NeuralAmpModeler.iss](NeuralAmpModeler/installer/NeuralAmpModeler.iss): bundle all 14 amp folders recursively
- Registry: `RigsPath` -> `{app}\rigs` (parent directory)
- [VoLumPaths.h](NeuralAmpModeler/VoLumPaths.h): `FindRigsDirectory()` returns `rigs/` root

---

## Phase 4: Polish

- Comic-style amp images (user generates via Gemini Imagen 3)
- Per-amp hero image in detail view
- VoLum branding (logo, title, about)
- README / user docs

---

## What user provides

- Channel labels and descriptions for all 14 amps
- Comic-style amp PNGs (one per amp, transparent background)
- VoLum logo (optional; can use text initially)
