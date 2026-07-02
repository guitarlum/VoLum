# U1 — Standalone native-window-border resize scales the UI

**Reported:** 1.2.0 standalone test feedback (Windows). **Priority:** medium (UX papercut; a working resize path already exists).

## Status: WON'T DO — reverted to 1.1.0 diagonal-grip resize

A native-border resize was implemented (override `OnParentWindowResize` +
`WS_THICKFRAME`/`WS_MAXIMIZEBOX` on `IDD_DIALOG_MAIN` + remove the corner grip)
and verified to scale the UI. **It was then reverted at the user's request:**
allowing all-edge native resize means off-aspect drags leave a letterbox/white
strip, which the user explicitly does not want.

**Final/desired behavior = 1.1.0:** the standalone window is a fixed,
non-sizable frame and the only resizer is the bottom-right corner grip
(`AttachCornerResizer(EUIResizerMode::Scale)`), which is inherently diagonal and
aspect-locked, so the UI always fills the window with no white frame. This is
the intended UX; native edge-resize is intentionally not offered.

Do not reopen unless the product decision changes (e.g. a future request for
free-axis resize with an aspect-lock via `WM_SIZING`, which would be an iPlug2
submodule change).

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
