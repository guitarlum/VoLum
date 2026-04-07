---
name: VoLum Full Rig Catalog
overview: Replace the Ampete-only flat dropdown with a hardcoded catalog of all 14 amps, a hierarchical selection UI (Amp Gallery -> Speaker Mode -> Channel), and an installer that bundles all rigs.
todos:
  - id: catalog-header
    content: Create VoLumCatalog.h with AmpInfo/ChannelInfo structs for all 14 amps (scaffold with placeholder labels for user to fill in)
    status: pending
  - id: params-hierarchy
    content: Replace single kVoLumAmpeteRig with kVoLumAmp + kVoLumSpeaker + kVoLumChannel params; update OnParamChange, rig loading, unserialization (v0.7.15)
    status: pending
  - id: paths-all-rigs
    content: "Update VoLumPaths.h: FindRigsDirectory() returns rigs/ root; load logic appends folder/speaker/channel from catalog"
    status: pending
  - id: ui-selectors
    content: "UI: amp prev/next selector, speaker mode buttons (4), channel buttons (dynamic count); standalone first"
    status: pending
  - id: installer-all-rigs
    content: Update .iss to bundle all 14 amp folders + update registry key to rigs/ root
    status: pending
  - id: images-integration
    content: Integrate amp images into UI gallery (after user provides PNGs)
    status: pending
  - id: branding
    content: Replace title/branding with VoLum product name (after user confirms name)
    status: pending
isProject: false
---

# VoLum: Full Rig Catalog + Hierarchical UI

## Current state

- 14 amp folders under `rigs/`, each with 4 speaker modes (AMP/G12/G65/V30) and 2--6 channels per amp
- Code only knows about "Ampete One" via a flat 16-entry enum in [VoLumAmpeteCatalog.h](NeuralAmpModeler/VoLumAmpeteCatalog.h)
- Installer only bundles `rigs\Ampete One\*.nam`
- UI is a single `IVMenuButtonControl` dropdown

## Rig inventory (from `rigs/`)

- Ampete One -- 4 channels (1-4)
- Bad Cat mini Cat -- 3 channels (1-3)
- Brunetti XL 2 -- 3 channels (1-3)
- Fryette Deliverance 120 -- 2 channels (3,4)
- H&K TriAmp Mk2 -- 6 channels (1-6)
- Lichtlaerm Prometheus -- 3 channels (1-3)
- Marshall 2204 1982 -- 6 channels (1-6)
- Marshall JMP 2203 1976 -- 6 channels (1-4,f=FatBee,x=FatBee+Clone)
- Marshall JVM 210H OD1 -- 6 channels (1-6)
- Orange OD120 1975 -- 5 channels (1-4,f)
- Orange ORS100 1972 -- 2 channels (1,2)
- Sebago Texas Flood -- 2 channels (1,2)
- Soldano SLO100 -- 3 channels (1-3)
- THC Sunset -- 5 channels (1-5)

Each amp x speaker mode x channel = one `.nam` file. Total ~224 files.

---

## Phase 1: Catalog data model

Replace [VoLumAmpeteCatalog.h](NeuralAmpModeler/VoLumAmpeteCatalog.h) with a new **`VoLumCatalog.h`** that describes all 14 amps.

**Data structure (compile-time, no JSON at runtime):**

```cpp
struct ChannelInfo {
  const char* suffix;       // filename suffix: "1", "2", "f", "x"
  const char* label;        // UI label: "Ch 1", "FatBee", "FatBee+Clone"
  const char* description;  // tooltip/detail: "Clean channel, low gain"
};

struct AmpInfo {
  const char* id;           // short ID used in filenames: "Ampt", "BadC", "2204"
  const char* folderName;   // disk folder: "Ampete One", "Marshall 2204 1982"
  const char* displayName;  // UI label: "Ampete One", "Marshall 2204 (1982)"
  const char* imageFile;    // e.g. "ampete_one.png" (for gallery)
  int channelCount;
  ChannelInfo channels[8];  // max 8, sized to largest (6 today + headroom)
};
```

- The 4 speaker modes (AMP, G12, G65, V30) are **global constants** (same for every amp), not per-amp data.
- File path is computed: `{rigsDir}/{folderName}/{speakerMode}-{id}-{suffix}.nam`
- You fill in labels/descriptions per channel; I scaffold the structure and you edit the strings.

**Changes to EParams:**
- Replace `kVoLumAmpeteRig` (single enum of 16) with **three params**:
  - `kVoLumAmp` -- enum 0..13 (which amp)
  - `kVoLumSpeaker` -- enum 0..3 (AMP / G12 / G65 / V30)
  - `kVoLumChannel` -- enum 0..N (channel within selected amp; max value changes per amp)
- `OnParamChange` for any of these three triggers rig reload.
- Unserialization: new version branch (bump to 0.7.15).

---

## Phase 2: UI layout (standalone first, then VST3)

The current window is **600 x 400**. The new flow replaces the bottom model-area strip with a multi-level selector. Rough layout concept:

```
+----------------------------------------------------------+
|  [VoLum logo / title]                          [settings] |
|                                                           |
|  [Input] [Threshold] [Bass] [Mid] [Treble] [Output]      |
|   knobs    knobs      knobs                               |
|  [Noise Gate]              [EQ]                           |
|                                                           |
|  +--- Amp selector (gallery or dropdown) ---------------+ |
|  |  [< prev]  "Marshall 2204 (1982)"  [next >]         | |
|  +------------------------------------------------------+ |
|  [AMP] [G12] [G65] [V30]    [Ch1] [Ch2] [Ch3] ...        |
|   speaker mode buttons        channel buttons             |
+----------------------------------------------------------+
```

- **Amp selector**: horizontal prev/next with amp name (and optionally a thumbnail). Could later become a full gallery popup.
- **Speaker mode**: 4 toggle buttons (AMP / G12 / G65 / V30); always 4.
- **Channel buttons**: dynamic count (2--6); rebuild when amp changes. iPlug2 can show/hide controls or use a `IVTabSwitchControl`.
- **No model file browser**, **no IR browser** on standalone. On VST3: optionally keep IR browser below if you want users to load custom IRs with AMP-only mode.

For images: you provide a `.png` per amp (around 100x80 or similar); we embed them as iPlug2 bitmap resources or load from the rigs folder at runtime.

> *This phase is where we iterate on look and feel together. The data model from Phase 1 is independent of the exact UI layout.*

---

## Phase 3: Installer + path resolution

- [NeuralAmpModeler.iss](NeuralAmpModeler/installer/NeuralAmpModeler.iss): change the `[Files]` source from `..\..\rigs\Ampete One\*.nam` to **`..\..\rigs\*`** (recurse all 14 folders).
- Registry key `AmpeteRigsPath` renamed to `RigsPath`, pointing at `{app}\rigs` (parent, not a single amp subfolder).
- [VoLumPaths.h](NeuralAmpModeler/VoLumPaths.h): `FindRigsDirectory()` returns the `rigs/` root; amp subfolder is appended from the catalog at load time.

---

## Phase 4: Polish and images

- You supply a photo/illustration per amp.
- Optional: per-amp background tint or full background swap when an amp is selected (deferred until basic gallery works).
- Branding: replace "NEURAL AMP MODELER" title with "VoLum" (or whatever you want).

---

## What you need to provide before implementation

- **Channel labels and descriptions** for all 14 amps (I can generate a template spreadsheet/table from the file listing for you to fill in).
- **Amp images** (one per amp, PNG, any reasonable size -- we resize in code).
- **Product name** for the title bar and UI header (VoLum? VoLum NAM Player? something else?).
