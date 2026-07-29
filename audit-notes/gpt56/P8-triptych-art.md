## Summary

The triptych geometry, motif bounds, deterministic RNG, colour helpers, and layer invalidation are generally disciplined: I found no reachable NaN/Inf path, unbounded loop, out-of-range motif index, or cross-thread RNG/state access in the assigned files. The highest-severity defect is the PRE-capture menu's visible scrollbar: its gutter is not a scrollbar hit target, so clicking it can select and load the pedal row behind it. I also found one repeatable hover/scroll mismatch and a UI-thread performance problem in the most expensive hero generators during amp changes and rescaling.

## Findings

### F-P8-1: MAJOR — Clicking the PRE-capture scrollbar selects the pedal row behind it

**Where:** `NeuralAmpModeler/VoLumTriptychMenus.h:131-139, 152-177`

**Mechanism:** When imported pedals make the menu overflow, `Draw()` paints a distinct scrollbar gutter and thumb at the right edge:

```cpp
if (scrollable)
{
  IRECT track(mRECT.R - sbW - 1.f, mRECT.T + 4.f, mRECT.R - 2.f, mRECT.B - 4.f);
  g.FillRect(IColor(40, 200, 162, 78), track);
  const float maxScroll = contentH - mRECT.H();
  const float thumbH = std::max(18.f, track.H() * (mRECT.H() / contentH));
  const float t = (maxScroll > 0.f) ? (mScroll / maxScroll) : 0.f;
  const float thumbY = track.T + (track.H() - thumbH) * t;
  g.FillRect(VoLumColors::GOLD_DIM, IRECT(track.L, thumbY, track.R, thumbY + thumbH));
}
```

But mouse-down explicitly discards `x` and resolves the click using only the row at `y`:

```cpp
void OnMouseDown(float x, float y, const IMouseMod& mod) override
{
  (void)mod;
  (void)x;
  const int idx = ItemIndexAtY(y);
  if (idx < 0 || idx >= static_cast<int>(mItems.size()) || mItems[static_cast<size_t>(idx)].isHeader)
    return;
  // ...
  plugin->_VolumSetPreNamCapture(mSlot, item.captureIdx);
  plugin->_VolumHidePreCaptureMenu();
}
```

There is no scrollbar exclusion, thumb hit-test, or drag state. Therefore the gutter is only visual; it is part of every row's effective mouse hit area even though row drawing stops to its left.

**Trigger:** Import enough custom pedals for the PRE-NAM capture menu to overflow, open the menu, then click or attempt to drag the visible scrollbar thumb over any non-header row.

**Impact:** The click loads the pedal at that vertical position and closes the menu instead of scrolling. This is a concrete wrong-selection path in a sound-affecting picker and makes the displayed scrollbar actively misleading.

**Fix sketch:** Define the scrollbar track/thumb geometry once and use it for drawing and hit-testing. Consume clicks in the gutter; implement thumb drag and track paging, or at minimum prevent gutter clicks from reaching row selection. This does not require a visual change.

**Proposed regression test:** `PreCaptureScrollbarClickDoesNotSelectRow` — with overflowing items, mouse-down inside the thumb/gutter must not invoke `_VolumSetPreNamCapture` and must not hide the menu; dragging the thumb must change `mScroll`.

### F-P8-2: MINOR — Wheel scrolling leaves the hover highlight attached to the old item

**Where:** `NeuralAmpModeler/VoLumTriptychMenus.h:143-149, 180-190`

**Mechanism:** Wheel handling changes `mScroll` and redraws, but does not recompute or clear `mHovered`:

```cpp
void OnMouseWheel(float x, float y, const IMouseMod&, float d) override
{
  if (MenuHeight(mItems) <= mRECT.H() + 0.5f)
    return;
  mScroll -= d * ItemHeight() * 1.5f;
  ClampScroll();
  SetDirty(false);
}
```

`mHovered` is only updated by a later mouse-move event:

```cpp
const int idx = ItemIndexAtY(y);
const int next =
  (idx >= 0 && idx < static_cast<int>(mItems.size()) && !mItems[static_cast<size_t>(idx)].isHeader) ? idx : -1;
if (next != mHovered)
{
  mHovered = next;
  SetDirty(false);
}
```

After the redraw, `Draw()` highlights item index `mHovered` at its new scrolled position, while a click at the stationary pointer resolves a different current index through `ItemIndexAtY(y)`.

**Trigger:** Hover a selectable row in an overflowing PRE-capture menu, scroll one or more wheel notches without moving the pointer, then click.

**Impact:** The highlighted row can be away from the pointer, and the subsequent click can select a different row than the one highlighted. Moving the pointer repairs the state, but wheel-only navigation is visibly inconsistent.

**Fix sketch:** After clamping in `OnMouseWheel`, recompute `mHovered` from the supplied `y` (and require `x` to be in the row area, excluding the scrollbar), or clear it until the next mouse move. No visual-design change is needed.

**Proposed regression test:** `PreCaptureWheelRecomputesHoverAtFixedPointer` — after a wheel event at a stationary row coordinate, assert that the highlighted index equals `ItemIndexAtY(y)` (or is `-1` by an explicit clear-on-wheel policy). It differs today.

### F-P8-3: MAJOR — Heavy hero variants regenerate synchronously during amp browsing and rescaling

**Where:** `NeuralAmpModeler/VoLumFractalArt.h:645-671, 860-875, 914-935`; trigger/cache path in `NeuralAmpModeler/VoLumHero.h:344-366, 222-230`

**Mechanism:** Several hero variants perform large amounts of work as individual `IGraphics` calls on the UI thread. The Brunetti art runs three fern passes totalling 26,000 iterations:

```cpp
fern(6000, (h * 0.78f) / 10.5f, h * 0.188f, rect.L + w * 0.55f, rect.T + h * 0.9f,
     IColor(255, 45, 90, 110), kMid, 0.9f, 0.32f, false);
fern(11000, (h * 0.86f) / 10.5f, h * 0.207f, rect.L + w * 0.5f, rect.T + h * 0.94f,
     IColor(255, 55, 120, 140), kTeal, 1.1f, 0.7f, false);
fern(9000, (h * 0.86f) / 10.5f, h * 0.207f, rect.L + w * 0.5f, rect.T + h * 0.94f,
     IColor(255, 55, 120, 140), kTeal, 1.15f, 0.55f, true);
```

The Orange OD120 and Soldano variants use a fixed 2-logical-pixel grid and run an escape-time loop for every sample:

```cpp
const float step = 2.f;
for (float px = 0; px < pw; px += step)
  for (float py = 0; py < ph; py += step)
  {
    // ...
    while (zr * zr + zi * zi < 4.0 && it < 40)
    {
      double t = zr * zr - zi * zi + cr;
      zi = 2 * zr * zi + ci;
      zr = t;
      it++;
    }
    if (it < 40 && it > 2)
      g.FillCircle(/* ... */);
  }
```

The Soldano path raises the inner cap to 64. In the normal 434x208 mono hero, padding produces a 422x196 art rect; the 82% grid is 174x81 = 14,094 samples, so the source proves worst-case caps of 563,760 and 902,016 escape iterations respectively, plus thousands of draw calls.

The output is cached, but only as the current mono layer. Changing the amp replaces that layer, so revisiting a heavy variant regenerates it. `OnRescale()` also explicitly nulls every hero layer:

```cpp
void OnRescale() override
{
  mMonoArtLayer = nullptr;
  mMainArtLayer = nullptr;
  mSupportArtLayer = nullptr;
  // ...
}
```

The next `DrawMonoHero()` immediately runs the generator synchronously. Thus resize/DPI notifications and browsing A -> B -> A are concrete repeated cache-miss paths.

**Trigger:** Select Brunetti XL 2, Orange OD120, or Soldano SLO100; switch to another amp and back, or drag/rescale the editor while one is selected.

**Impact:** The iteration and draw-call counts are proved by the source and all run on the host UI thread. The exact stall duration is backend/hardware-dependent (not measured in this read-only audit), but these paths can miss frame budgets badly and make amp auditioning or resize feel frozen. The cost is bounded, so this is not an infinite-loop finding.

**Fix sketch:** Preserve the exact rendered result but avoid synchronous repeated generation: cache immutable art per `(fractalCase, logical bounds, backing scale)` rather than only the current amp, rasterize escape-time variants into a pixel buffer/bitmap instead of issuing one vector draw per accepted sample, and avoid rebuilding on every transient resize tick (show a scaled cached layer during the gesture, then rebuild once at the settled scale). These approaches need not alter the art; changing sample density would alter it and should be a separate human-approved option.

**Proposed regression test:** `HeroFractalCacheSurvivesAmpRoundTripAndCoalescesRescale` — instrument generator invocations and assert A -> B -> A generates case A once per settled scale, not twice, and a resize gesture generates at most once after settling. Add a counted draw-backend test asserting the escape-time implementation does not emit one vector primitive per accepted pixel.

## Visual-change-required observations (report only)

None. All three fixes can preserve the current composition, motifs, and colours. Reducing fractal sample density would change the rendered texture, so I am not recommending that as the default performance fix.

## Areas read and found clean

- `NeuralAmpModeler/VoLumTriptych.h` — read in full (1-1017). Frame selection, slot/focus array indexing, hit-test precedence, parameter bounds for the fixed slot tables, motif layer keys `(focus, bypass, variant)`, and `CheckLayer`-based scale/RECT invalidation were consistent. No reachable invalid enum index or null parameter was found on current call paths.
- `NeuralAmpModeler/VoLumTriptychMenus.h` — read in full (1-255). Row-height arithmetic, clipping, scroll clamping, selected-index handling (including custom legacy indices), and menu height capping were sound apart from F-P8-1 and F-P8-2.
- `NeuralAmpModeler/VoLumTriptychLayout.h` — read in full (1-163). For the production fixed logical triptych bounds, all three frame states preserve the 686px invariant and produce positive, non-overlapping card rectangles. The Scale-mode editor resizer scales these logical coordinates rather than feeding arbitrary tiny window dimensions into this helper.
- `NeuralAmpModeler/VoLumTriptychMotifs.h` — read in full (1-388). All production loop bounds are finite; divisions have fixed non-zero denominators on reachable paths; the reverb point set is seeded before modulo indexing; and motif coordinates remain finite for production card/slot rects. Local deterministic RNG avoids shared-state races. `DrawNeuralNetMotif` currently has no production caller, so its generic unchecked `layers/L` inputs have no concrete trigger and were not reported.
- `NeuralAmpModeler/VoLumFractalArt.h` — read in full (1-1108, longer than the supplied estimate). RNG state is local, amp-index mapping is range-checked at production call sites, custom-art modulo handles negative IDs, loop/recursion depths are bounded, and normalization/square-root inputs are non-negative on reachable paths. No reachable NaN/Inf coordinate path or non-terminating loop was found. `DrawStripMiniFractal` currently has no production caller and was not used to manufacture findings.
- `NeuralAmpModeler/VoLumColorHelpers.h` — read in full (1-324). Production colour/gradient arguments are in domain; selection and depth helpers contain no data-dependent loops or mutable shared state.
- `NeuralAmpModeler/Colors.h` — read in full (1-86). Constants are valid ARGB values and have no indexing, arithmetic, lifetime, or threading hazards.
