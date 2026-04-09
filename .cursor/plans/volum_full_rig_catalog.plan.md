---
name: VoLum Full Rig Catalog
overview: VoLum NAM Player -- 14 bundled amps with persistent sidebar UI, speaker/channel selection, comic-style amp artwork, shipped via Windows installer for standalone and VST3.
isProject: false
---

# VoLum: Full Rig Catalog + Hierarchical UI

**Product name:** VoLum  
**Tagline:** NAM PLAYER  
**Based on:** [Neural Amp Modeler Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) (iPlug2 / NanoVG)

---

## What VoLum Is

VoLum is a repackaged NAM plugin that ships 14 bundled guitar amp profiles ("rigs") with a new UI purpose-built for browsing, selecting, and playing them. It is **not** a general-purpose NAM loader -- it does not expose file browsers. Instead, it presents a curated amp gallery and lets users pick amp, speaker mode, and gain stage with dedicated controls.

The target audience is guitarists who want a plug-and-play experience: install, pick an amp, play. No hunting for `.nam` files.

---

## Rig File Pattern (critical knowledge)

All rigs live under `rigs/` with one subfolder per amp. The naming convention is:

```
rigs/
  {AmpFolder}/
    {Speaker}-{AmpCode}-{Channel}.nam
```

### Speaker prefixes (4 modes)

| Prefix | Meaning                    |
|--------|----------------------------|
| `AMP`  | Direct amp signal, no cab  |
| `G12`  | Celestion G12-65 cabinet   |
| `G65`  | Celestion G12-65 variant   |
| `V30`  | Celestion Vintage 30 cab   |

### Channel suffixes

The suffix after the last `-` identifies the gain stage / channel setting of the amp when it was profiled. These vary per amp:

- Numeric: `1`, `2`, `3`, `4`, `5`, `6` (ascending gain stages)
- Special: `f` (FatBee mod), `x` (FatBee + Clone mod)

### Concrete example: Marshall JMP 2203 1976

```
rigs/Marshall JMP 2203 1976/
  AMP-2203-1.nam    AMP-2203-2.nam    AMP-2203-3.nam    AMP-2203-4.nam
  AMP-2203-f.nam    AMP-2203-x.nam
  G12-2203-1.nam    G12-2203-2.nam    G12-2203-3.nam    G12-2203-4.nam
  G12-2203-f.nam    G12-2203-x.nam
  G65-2203-1.nam    ...
  V30-2203-1.nam    ...
```

That's 4 speakers x 6 channels = 24 files for this amp.

### Full amp inventory (14 amps)

| Amp Folder                   | Display Name       | Channels per speaker |
|------------------------------|--------------------|----------------------|
| Ampete One                   | Ampete One         | 4 (1-4)              |
| Bad Cat mini Cat             | Bad Cat mini Cat   | 3 (1-3)              |
| Brunetti XL 2                | Brunetti XL 2      | 3 (1-3)              |
| Fryette Deliverance 120      | Fryette Deliv. 120 | 2 (3, 4)             |
| H&K TriAmp Mk2              | H&K TriAmp Mk2    | 6 (1-6)              |
| Lichtlaerm Prometheus        | Lichtlaerm Prom.   | 3 (1-3)              |
| Marshall 2204 1982           | Marshall 2204      | 6 (1-6)              |
| Marshall JMP 2203 1976       | Marshall JMP 2203  | 6 (1-4, f, x)        |
| Marshall JVM 210H OD1        | Marshall JVM       | 6 (1-6)              |
| Orange OD120 1975            | Orange OD120       | 5 (1-4, f)           |
| Orange ORS100 1972           | Orange ORS100      | 2 (1, 2)             |
| Sebago Texas Flood           | Sebago Texas Fl.   | 2 (1, 2)             |
| Soldano SLO100               | Soldano SLO100     | 3 (1-3)              |
| THC Sunset                   | THC Sunset         | 5 (1-5)              |

Each amp has the same 4 speaker modes. Total ~224 `.nam` files.

---

## Architecture & Key Design Decisions

### 1. No EParams for amp/speaker/channel (callback-based state)

**Decision:** Amp, speaker, and channel selection are stored as plain member variables (`mVolumAmpIdx`, `mVolumSpeakerIdx`, `mVolumChannelIdx`) on the plugin class, NOT as iPlug2 `EParam` entries.

**Rationale:**
- The number of channels varies per amp (2 to 6). iPlug2 params are fixed at init time -- you can't change an enum param's range dynamically.
- Speaker mode is orthogonal to channel: changing speaker recalculates the available channels. A flat enum of all combinations would be 224 entries and would make the UI confusing.
- The controls (`VoLumChannelStepControl`, `VoLumSpeakerRowControl`) use callbacks that directly update the member state and set an atomic `mVolumNeedsLoad` flag, bypassing the param system entirely.

**Update:** `SerializeState` / `UnserializeState` (and standalone user-profile `volum-settings.json`, with legacy read of `rigs/volum-settings.json`) persist amp/speaker/channel plus per-amp knob/toggle snapshots. The legacy `kVoLumAmpeteRig` param remains initialized for backward compat with older saved state but is not used at runtime.

**Open question:** Should amp/speaker/channel be proper params for DAW automation? If so, the param ranges would need to be worst-case (max 14 amps, 4 speakers, 8 channels) with clamping.

### 2. Dynamic channel discovery (filesystem scan)

**Decision:** Available channels for a given amp + speaker combo are discovered at runtime by scanning the amp's folder for files matching `{Speaker}-*.nam`.

**Rationale:**
- Avoids maintaining a second source of truth (the directory IS the catalog).
- Handles future amp additions automatically.
- The scan is trivial: < 30 files per folder, < 1ms on any modern filesystem.

**Implementation:** `volum::DiscoverChannels()` in `VoLumPaths.h`. Returns sorted `{filename, label}` pairs. Labels are the uppercased channel suffix.

**Trade-off:** Requires the rigs directory to be present and accessible. If missing, the channel list is empty and no model loads.

### 3. Background model loading (non-blocking UI)

**Decision:** NAM model loading is offloaded to a `std::thread` to keep the UI responsive.

**Rationale:**
- `nam::get_dsp()` parses JSON + allocates Eigen matrices, taking 200-500ms per model.
- Blocking the UI thread during this makes the app feel broken.

**Implementation:**
1. Any state change (amp/speaker/channel click) sets `mVolumNeedsLoad = true` (atomic).
2. `OnIdle()` (main thread, ~60Hz) checks the flag. If set and no load in progress, it captures the full file path and spawns a detached thread.
3. The thread calls `_StageModel()`, which stores the model in `mStagedModel`.
4. `ProcessBlock()` (audio thread) calls `_ApplyDSPStaging()`, which atomically moves `mStagedModel` to `mModel`.
5. Footer label updates immediately with the target filename so users can see what's loading.

**Trade-off:** There's a brief moment where the old model continues playing while the new one loads. This is preferable to silence or clicks.

### 4. Single-view UI with persistent sidebar (not gallery + detail)

**Decision:** The original two-view design (gallery page → detail page) was replaced with a single view: persistent amp list sidebar on the left, detail/controls panel on the right.

**Rationale:**
- With only 14 amps, a full-page gallery is overkill. They all fit in a sidebar list without scrolling.
- Eliminates navigation friction: the user can switch amps with one click, without "going back" to a gallery.
- The detail panel always shows the currently selected amp's image, name, speaker mode, channel, and all controls.

**Trade-off:** Less visual impact than a full-page gallery with large thumbnails. But the user explicitly preferred this after seeing both designs.

### 5. Speaker mode buttons instead of a dropdown

**Decision:** The 4 speaker modes (AMP / G12 / G65 / V30) are always-visible pill buttons at the top of the detail panel, split into "DIRECT" (AMP) and "CABINET" (G12, G65, V30) groups.

**Rationale:**
- Only 4 options -- buttons are faster than a dropdown.
- Visual grouping communicates the semantic difference: AMP = no cab sim, others = cab sim baked into the NAM profile.
- Default is V30 because most users expect cab simulation and "AMP only" sounds harsh without an external IR.

### 6. Channel stepper instead of knob

**Decision:** The channel/gain-stage selector is a discrete `< CH 1 >` stepper control, not a rotary knob.

**Rationale:**
- Channels are discrete values (2-6 per amp), not continuous. A knob with 3 steps feels wrong and has dead zones.
- A stepper makes the discrete nature obvious and avoids the "laggy" feeling of a knob trying to snap to few values while triggering model loads at intermediate positions.
- Arrows are always visible (not hover-only) for discoverability.

### Keyboard shortcuts (VoLum)

- **Up / Down** — select previous / next amp in catalog order (same as moving the sidebar); persists via per-amp settings save.
- **Left / Right** — decrement / increment the channel stepper (wraps within the current amp + speaker’s channel list).
- Implemented in `NeuralAmpModeler.cpp` via `IGraphics::SetKeyHandlerFunc` (VK 0x26–0x28). VST3 users may need click focus; some hosts consume arrow keys globally.

### 7. Conditional compilation via `VOLUM_AMPETE_PRODUCT`

**Decision:** All VoLum-specific code is gated behind `#if VOLUM_AMPETE_PRODUCT` (defined in `config.h`).

**Rationale:**
- The same codebase can still build the original NAM plugin by setting `VOLUM_AMPETE_PRODUCT 0`.
- Keeps the diff clean: VoLum additions don't touch the original NAM code paths.

---

## File Map (VoLum-specific files)

| File | Purpose |
|------|---------|
| `NeuralAmpModeler/config.h` | `VOLUM_AMPETE_PRODUCT 1`, window size 900x600 |
| `NeuralAmpModeler/VoLumAmpeteCatalog.h` | Amp names, folder names, speaker prefixes, legacy rig arrays |
| `NeuralAmpModeler/VoLumPaths.h` | Rig directory discovery, channel file scanning |
| `NeuralAmpModeler/VoLumControls.h` | All custom iPlug2 controls (sidebar, speaker row, channel stepper, hero image, footer, etc.) |
| `NeuralAmpModeler/NeuralAmpModeler.h` | Plugin class with VoLum state members |
| `NeuralAmpModeler/NeuralAmpModeler.cpp` | Plugin impl: layout, callbacks, loading, OnIdle |
| `NeuralAmpModeler/installer/VoLum.iss` | Windows installer (standalone + VST3 + all amp rig folders) |
| `NeuralAmpModeler/installer/changelog.txt` | Version history |
| `rigs/` | All amp folders with `.nam` files (not in git, bundled by installer) |

---

## Remaining Work

### Must-have before release
- [ ] **Hero art:** comic-style / final amp images for every model in the sidebar + hero area
- [ ] Per-amp **human-readable channel labels** (e.g. “Clean”, “Lead”) instead of only numeric / suffix codes where desired

### Done (keep for history)
- [x] Per-amp persistence: knobs, toggles, speaker, channel; JSON + VST3 state (see `Unserialization.cpp` / changelog)
- [x] User-facing README with amp table, rig naming, dev file map

### Nice-to-have
- [ ] DAW **automation** of amp / speaker / channel (would need worst-case param ranges or a different strategy)
- [ ] macOS build and installer
