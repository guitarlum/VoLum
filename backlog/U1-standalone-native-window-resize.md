# U1 — Standalone native-window-border resize scales the UI

**Reported:** 1.2.0 standalone test feedback (Windows). **Priority:** medium (UX papercut; a working resize path already exists).

## Status: RESOLVED (core) on `feature/1.2.0`

Implemented Option 1 (forward native resize to IGraphics) **without** an iPlug2
submodule change: `NeuralAmpModeler::OnParentWindowResize` (APP-only) maps the
native `WM_SIZE` to an aspect-locked `IGraphics::Resize(..., needsPlatformResize=false)`,
and the standalone corner grip was removed (`AttachCornerResizer` is now
`#ifndef APP_API`) to eliminate the grip/native-border overlap that could hang
the app. Verified: native resize scales the UI to fill, responsive, no hang.

**Remaining (optional):** dragging off the UI aspect ratio leaves a thin
letterbox strip. Eliminating it entirely requires constraining the drag to the
UI aspect via `WM_SIZING` in `IPlugAPP_dialog.cpp` (the submodule change below).
Only pursue if the letterbox is judged worth a mirror-branch + submodule bump.

## Problem

In the standalone app there are effectively two resize edges:

- **Inner (works):** the in-canvas corner grip (`AttachCornerResizer(EUIResizerMode::Scale, ...)` at `NeuralAmpModeler.cpp:440`) scales the whole UI.
- **Outer (broken):** dragging the native OS window border grows the window but does **not** scale `IGraphics`, leaving a white/empty frame around the UI. The two edges are hard to tell apart, so users drag the outer one and think resize is broken.

Desired: dragging the native window border scales the UI the same way the corner grip does (or, at minimum, the two behave consistently). Sidebar amp thumbnails / hero art must also rescale on this path (B3 — `OnRescale` layer invalidation already added in `VoLumAmpList.h`/`VoLumHero.h`, but unverified because the native path never rescales).

## Why it's not a quick fix

- `PLUG_HOST_RESIZE 0` (`config.h`). The standalone window resize lives in the iPlug2 **APP** wrapper (`iPlug2/IPlug/APP/*`), which is a **submodule** consumed via the private `guitarlum/iPlug2` mirror (see `NeuralAmpModeler/iplug2-patches/README.md`). Any change must go through the mirror branch + submodule-pointer bump, and touches cross-platform window handling (Win32 `WM_SIZE` -> `IGraphics::Resize` with scale, plus min/max + aspect constraints; macOS parity).

## Options to evaluate

1. **Forward native resize to IGraphics:** handle native `WM_SIZE` (and macOS equivalent) in the APP host so border drags call `IGraphics::Resize(w, h, scale)`, honoring `PLUG_MAX_WIDTH/HEIGHT` and aspect lock. Closest to user expectation.
2. **Disable free native resize:** drop the resizable native frame so only the corner grip resizes. Lowest risk, removes the confusing white frame, but loses native-edge resizing.

## Acceptance criteria

- Standalone: dragging the native window border scales the UI with no white frame, OR native border resize is cleanly disabled and only the corner grip remains — decision recorded.
- Sidebar thumbnails + hero art rescale crisply on whichever resize path ships (verify B3 here).
- Aspect ratio / min / max respected; no clipping or smearing at extremes.
- macOS parity checked (manual note in the macOS smoke checklist).
- iPlug2 change committed in the private mirror and pinned by the parent submodule SHA per `iplug2-patches/README.md`.

Work on a dedicated branch off latest `dev`. Do not promote to `main` outside a release.
