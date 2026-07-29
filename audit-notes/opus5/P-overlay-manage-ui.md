I've read every line of the eight files in scope plus the store/API/plugin code they depend on. Here are the findings, ranked.

---

## 1. BLOCKER — Per-row action codes collide once a list has ≥100 rows: the pen deletes a *different* item

**WHERE** `NeuralAmpModeler/VoLumCustomOverlay.h:377-387` (bases), `:715-746` (dispatch), `:1318-1343` (registration)

**MECHANISM** Row identity is packed into the action int with bases only 100 apart, but the row count is unbounded:

```377:387:NeuralAmpModeler/VoLumCustomOverlay.h
    kRowBase = 100, // Manage row body (select / double-click primary action)
    kRowOverwriteBase = 500, // Manage inline [overwrite] icon (presets only)
    kRowRenameBase = 600, // Manage inline [pen] icon
    kRowDeleteBase = 700, // Manage inline [trash] icon
    kRowIrCfgBase = 800, // Manage inline [gear] icon (IR only): open shaping editor
```

Every row registers `kRowRenameBase + i`, `kRowDeleteBase + i`, `kRowIrCfgBase + i` with no cap on `i` (`:1320`, `:1324`, `:1334`), and dispatch tests fixed 100-wide windows in this order:

```724:740:NeuralAmpModeler/VoLumCustomOverlay.h
    if (action >= kRowRenameBase && action < kRowRenameBase + 100)
    { mSel = action - kRowRenameBase; HandleManageAction(kRename, rect); return; }
    if (action >= kRowDeleteBase && action < kRowDeleteBase + 100)
    { mSel = action - kRowDeleteBase; HandleManageAction(kDelete, rect); return; }
    if (action >= kRowIrCfgBase && action < kRowIrCfgBase + 100)
    { OpenIrSettingsPopup(action - kRowIrCfgBase, rect); return; }
```

So for row 100+: pen → `600+100 = 700` → matches the **delete** window → `mSel = 0` → delete-confirm for row 0. Trash → `800` → matches the **gear** window → opens the IR popover for row 0. Gear → `900` → matches nothing (dead). Rows 256-399: the body click is dead too. Rows 400+: body click → `500+` → the **overwrite** branch → `mOverwritePreset(i-400)`, which is wired to `_VolumOverwritePreset` regardless of `mManageKind`, so clicking an *IR* row body destroys a saved *preset* snapshot. Ironically the Builder half of the same file documents the invariant that is being violated here (`:891-892`, "each span must stay < 100").

Deleting the wrong IR is real data loss, not just a wrong dialog: `DeleteIR` → `Store::RemoveIR` (`VoLumContentStore.h:845-867`) deletes the copied WAV from the content library and clears `activeIrId`/`supportActiveIrId` in every stored scene and preset.

**TRIGGER** Manage custom IRs → `+ Import IR (.wav)` → multi-select an IR pack of 100+ files (one dialog, one click) → scroll to row 101 → click its pen → dialog says `Delete Custom IR "<name of row 1>"` → Enter. Same with 100+ presets accumulated over time.

**IMPACT** With a large library, the per-row icons act on the wrong item or do nothing: renaming deletes, deleting opens someone else's shaping popover, the gear is inert, and (≥400 rows) selecting a row overwrites a preset. The confirm dialog is the only thing between the user and losing an IR they didn't point at.

**CONFIDENCE** certain (arithmetic; no runtime ambiguity)

**FIX SKETCH** Stop packing the index into the action code: give `AddHotspot` a third `index` field and dispatch on `{action, index}`. If that's too invasive for 1.2.1, move the bases to 4096-apart constants and add a `static_assert`-style guard plus a hard cap on registered rows. No audio impact.

**Would an existing test have caught it?** No. `test_volum_ui_regressions.cpp` only string-pins the hover wiring (`:550-553`) and the NAM-import calls (`:1084-1087`); nothing exercises action-code dispatch, and `test_volum_content_crud_edge.cpp` tests the store API directly, never the overlay's code mapping.

---

## 2. MAJOR — Manage rows register click targets outside the list viewport: clicking the footer hint deletes an IR

**WHERE** `NeuralAmpModeler/VoLumCustomOverlay.h:1300-1370` (vs. the Builder's guard at `:1463`)

**MECHANISM** The Manage loop culls only rows that are *fully* outside the viewport, then registers hotspots from the raw row rect:

```1302:1320:NeuralAmpModeler/VoLumCustomOverlay.h
        const IRECT row(listArea.L + 4.f, y, listArea.R - 4.f - sbW, y + rowH - 4.f);
        y += rowH;
        if (row.B < listArea.T || row.T > listArea.B)
          continue;
...
        const IRECT trash(ix, row.T, ix + iconW, row.B);
        DrawBinGlyph(g, trash, VoLumColors::CREAM_DIM);
        AddHotspot(trash, kRowDeleteBase + i, deleteTip.c_str());
```

Drawing is clipped to `listArea` (`:1297`), but hit testing is not, so a partially scrolled row's trash/pen/gear/body remain clickable for up to 28 px beyond the viewport. The Builder list, in the same file, explicitly guards exactly this (`const bool rowVisible = ...; if (rowVisible) AddHotspot(...)`, `:1463-1511`) — the Manage list was never given the same treatment.

Geometry: `listArea` ends 26 px above the panel body's bottom (`:1279`), and the error/hint line occupies the last 22 px (`:1387`). Nothing else registers a hotspot in that strip, so the clipped bottom row owns it.

**TRIGGER** Manage custom IRs with ≥9 entries → scroll so the bottom row is half-cut → click the hint text `Import a .wav, then double-click…` at the horizontal position of the trash glyph → "Delete Custom IR" confirm for a row you can barely see. Double-clicking the same spot instead fires `mPrimaryAction` → the IR is convolved onto the focused cab and the overlay closes (an audible change from clicking a help label). The 10 px gap between the Import button and the list top behaves the same way for the top clipped row.

**IMPACT** Destructive and audio-changing actions fire from clicks on non-interactive chrome, attributed to a row the user can't fully see.

**CONFIDENCE** certain

**FIX SKETCH** Copy the Builder's guard: compute `rowVisible` and skip all four `AddHotspot` calls (`:1320`, `:1324`, `:1334`, `:1341`, `:1370`) for rows not fully inside `listArea`. Draw-only change; no audio.

**Would an existing test have caught it?** No — there is no hit-test harness for the overlay at all; the Builder's version of this guard is also unpinned.

---

## 3. MAJOR — Typed IR values silently reject the European decimal comma, and the parse is locale-fragile

**WHERE** `NeuralAmpModeler/VoLumContentStore.h:245-277`, reached from `VoLumCustomOverlay.h:1051-1072`

**MECHANISM** The sanitizer strips whitespace and lowercases, then hands the string to `strtod` and demands the remainder be a known unit:

```256:271:NeuralAmpModeler/VoLumContentStore.h
  const char* begin = s.c_str();
  char* end = nullptr;
  const double raw = std::strtod(begin, &end);
  if (end == begin || !std::isfinite(raw))
    return {};
  std::string suffix(end);
  ...
  if (!(suffix.empty() || suffix == "hz" || suffix == "db"))
    return {};
```

`strtod` honours `LC_NUMERIC`. Nothing in the tree calls `setlocale` (I checked all of `NeuralAmpModeler/`, `iPlug2/`, `NeuralAmpModelerCore/`, `AudioDSPTools/`), so the process runs in the `"C"` locale and `,` is never a decimal point: `"2,5k"` → `strtod` consumes `2`, leaves `",5k"` → unrecognised suffix → `Invalid` → `ApplyTypedIr*` returns `current` (`:287`, `:298`, `:309`). The popover redraws the old value with no error banner — the user sees a field that just ignored them. The failure is bidirectional: a host or a co-loaded plugin that calls `setlocale(LC_ALL, "")` (a long-standing VST hazard) flips it so `"2.5k"` becomes the broken spelling instead.

The same class of bug reaches the knob exact-entry in scope here: `VoLumExactEntry.h:135` routes through `IParam::StringToValue`, which is `atof(str)` (`iPlug2/IPlug/IPlugParameter.cpp:368`), so `0,5` typed into the exact-value box silently yields `0`.

**TRIGGER** Manage custom IRs → gear → click the High cut value → type `2,5k` → Enter. Nothing changes, nothing is reported.

**IMPACT** A German user (this project's primary user) types the decimal separator their keyboard and OS use and the new 1.2.1 typed-entry feature silently no-ops. `2.5K` uppercase, `1e9`, and leading zeros all work; `2,5k` and `2,5` do not.

**CONFIDENCE** certain for the comma; likely for the host-flipped-locale inverse

**FIX SKETCH** In the sanitizing loop at `:249-251`, map `','` to `'.'` alongside the whitespace strip; that makes both spellings work in either locale. Optionally switch to `std::from_chars` (locale-independent) for full immunity. Parser-only change; no audio. Consider surfacing `IrTypedKind::Invalid` in `mError` so a rejected entry isn't silent.

**Would an existing test have caught it?** No. `test_volum_ir_shaping.cpp:116-134` checks `"2500"`, `"2.5k"`, `"2.5 kHz"`, `"-3 dB"`, `"banana"`, `"2.5 furlongs"` — every case uses a period, and no case runs under a non-C locale.

---

## 4. MAJOR — The gold "this IR is shaped" gear is gold for every IR, including untouched ones

**WHERE** `NeuralAmpModeler/VoLumCustomOverlay.h:1331-1333`, with `VoLumSceneRig.inc.cpp:789-813`

**MECHANISM** The indicator treats any nonzero trim as user shaping:

```1331:1333:NeuralAmpModeler/VoLumCustomOverlay.h
          const volum::custom::IRShaping s = volum::custom::IRShapingAt(i);
          const bool shaped = (s.trimDb != 0.0) || (s.lowCutHz > 0.0) || (s.highCutHz > 0.0);
          DrawGearGlyph(g, gear, shaped ? VoLumColors::GOLD : VoLumColors::CREAM_DIM);
```

But 1.2.1's own auto-normalization writes a nonzero trim to *every* IR: `_VolumMigrateIrTrims` sets `ir.trimDb = AutoNormalizeIrTrimDb(sqrt(sumSq))` = `18 - 20*log10(L2)` for each uncalibrated entry, and it runs from the overlay's `changedCb` (`VoLumLayoutBuild.inc.cpp:1251`) — i.e. after the first import or the first stepper click. `L2 == 1.0` exactly is the only value that yields 0 dB, which no real IR hits.

**TRIGGER** Import any IR → reopen Manage custom IRs → its gear is already gold, though nothing was shaped.

**IMPACT** The one at-a-glance affordance the 1.2.1 IR view adds carries zero information: all gears are gold from the moment the library is calibrated, so "which IRs did I edit?" is unanswerable.

**CONFIDENCE** certain

**FIX SKETCH** Distinguish calibration from user intent. Smallest safe change: record the auto-normalized baseline alongside the trim (an additive `autoTrimDb` key — the v3 reader is already forward-tolerant per `VoLumContentStore.h:72-76`) and treat trim as shaped only when `trimDb != autoTrimDb`. **Must not alter `trimDb` itself** — this is a glyph-colour fix only, so voicing is untouched.

**Would an existing test have caught it?** No. `test_volum_ir_shaping.cpp` covers auto-normalize and the ladders as separate units; nothing asserts the relationship between the auto-normalized trim and the "shaped" predicate, which lives in the untested Draw path.

---

## 5. MAJOR — Deleting a non-focused custom amp reverts the sidebar and hero to a factory amp while the custom amp keeps playing

**WHERE** `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:163-201` (sidebar trash → confirm), with `VoLumAmpList.h:66-71`, `:142`, `:594`

**MECHANISM** The delete callback unconditionally clears the sidebar selection and repaints the hero from the factory amp, then only *shifts* the focus index:

```168:191:NeuralAmpModeler/VoLumLayoutBuild.inc.cpp
              list->SetCustomAmps(volum::custom::MockCustomAmps(), volum::custom::MockCustomAmpArts());
              list->SetCustomSelected(-1);
...
              h->SetPlaceholder(ph, mVolumAmpIdx);
              h->SetName(volum::kAmps[mVolumAmpIdx].displayName);
...
            if (mVolumCustomMainIdx == customIdx)
              mVolumCustomMainIdx = -1;
            else if (mVolumCustomMainIdx > customIdx)
              --mVolumCustomMainIdx;
```

When the deleted amp is *not* the focused one, `mVolumCustomMainIdx` stays `>= 0` (just decremented) — the custom amp is still loaded and `_VolumSyncPresetOwner()` still publishes its preset bank — but `mCustomSelected == -1` makes `VoLumAmpList.h:142` highlight the factory row again and the hero/sub-row now display the factory amp's name and placeholder art.

**TRIGGER** Have ≥2 custom amps. Select custom amp #2. Hover custom amp #1 → trash → Delete.

**IMPACT** The sidebar highlight, hero art and amp name all claim a factory amp while the custom amp is what you hear and what the preset bar is banked to. Clicking around to "fix" it is likely to load something else.

**CONFIDENCE** likely (the desync is clear from this code path; I did not find a later re-sync, but `_VolumSyncUiFromState` is only invoked on UI open)

**FIX SKETCH** In `doDelete`, branch on `mVolumCustomMainIdx == customIdx`: only reset hero/name/selection in that case; otherwise call `list->SetCustomSelected(shiftedIdx)` after the index fixup and leave the hero alone. No audio change.

**Would an existing test have caught it?** No. `test_volum_ui_sync_plan.cpp` exercises the sync planner, but this callback bypasses it and hand-writes the hero/selection updates.

---

## 6. MINOR — The Level stepper rounds before stepping, so it skips a 0.5 dB rung from a typed value

**WHERE** `NeuralAmpModeler/VoLumContentStore.h:137-141` (vs. the documented rule at `:154-159`)

**MECHANISM** The cut ladders deliberately move to the strictly-adjacent rung ("Deliberately not 'round to nearest, then move by one'… rounding first would silently skip the rung the user is standing next to"). The trim does the opposite:

```137:141:NeuralAmpModeler/VoLumContentStore.h
inline double StepIrTrimDb(double db, int dir)
{
  const double stepped = db + (dir >= 0 ? 0.5 : -0.5);
  return ClampIrTrimDb(std::round(stepped * 2.0) / 2.0); // snap to a 0.5 dB grid
}
```

From a typed `12.3`: up → `round(12.8*2)/2 = 13.0`, skipping the `12.5` rung directly above; down → `12.0`. From `12.6`: down → `12.0`, skipping `12.5`. So `+` then `−` does not return you to where you were, and the increment silently varies between 0.4 and 0.9 dB.

**TRIGGER** Gear → click Level → type `12.3` → Enter → click `+`. Value jumps to `+13.0 dB`.

**IMPACT** The typed-then-stepped interaction the release note promises ("stepping from a typed value must move to the next ladder rung rather than rounding first") is only true for the two cut fields.

**CONFIDENCE** certain

**FIX SKETCH** `dir >= 0 ? std::floor(db * 2.0 + 1.0) / 2.0 : std::ceil(db * 2.0 - 1.0) / 2.0`, then clamp. Same 0.5 grid, strictly adjacent. This changes only where a *user-initiated* step lands after typing — no voicing change, but it does move an audible level, so mention it in the changelog.

**Would an existing test have caught it?** No — worse, it pins the bug: `test_volum_ir_shaping.cpp:56` asserts `StepIrTrimDb(3.3, +1) == 4.0`, exactly the rung-skip. The adjacent-rung guarantee is only tested for the cut ladders (`:162-165`).

---

## 7. MINOR — An empty or whitespace-only commit resets Level to 0 dB, discarding up to 24 dB of calibration

**WHERE** `NeuralAmpModeler/VoLumContentStore.h:253-254`, `:280-289`

**MECHANISM** `if (s.empty() || s == "off" || s == "-" || s == "none") return {IrTypedKind::Off, 0.0};` — the sanitizer has already stripped all whitespace, so `"   "` is also `Off`, and `ApplyTypedIrTrimDb` maps `Off` to `0.0`. Since the entry opens with `SelectAll()` (`iPlug2/IGraphics/Controls/ITextEntryControl.cpp:536`), one Backspace empties it, and *any* click outside the box commits rather than cancels (`:157-163`). A bare `-` — the first keystroke of a negative number — commits the same way.

**TRIGGER** Gear on an imported IR → click Level (shows e.g. `+18.0 dB`) → Backspace → click anywhere → the IR drops ~18 dB with no undo.

**IMPACT** Silent loss of the auto-normalized level; the IR suddenly plays far quieter than the stock cabs and the user has no way to recover the number.

**CONFIDENCE** certain

**FIX SKETCH** Treat empty/whitespace-only as `Invalid` (keep current value — an empty field means "never mind"), and keep `off`/`none`/`0` as the explicit reset. Consider dropping `"-"` from the `Off` set for the same reason. Parser-only.

**Would an existing test have caught it?** No. `test_volum_ir_shaping.cpp:128` asserts `ParseIrTypedValue("") == Off`, i.e. it pins this behaviour as intended; whitespace-only and `"-"` are untested.

---

## 8. MINOR — Escape and outside-click dismiss different things when the IR popover is open

**WHERE** `NeuralAmpModeler/VoLumCustomOverlay.h:199-206`, `:359-364`; `VoLumLayoutBuild.inc.cpp:1355-1371`

**MECHANISM** The 1.2.1 fix makes an outside click close *only* the popover (and correctly swallows the click, so the row behind it is not hit). Escape has no popover-aware path: the global handler finds the overlay un-hidden and calls `c->Hide(true)`, whose override runs `ResetTransient()` — so one Escape closes the popover *and* the whole Manage panel.

**TRIGGER** Manage custom IRs → gear → Esc. The panel is gone, not just the popover.

**IMPACT** Inconsistent dismissal; users who reach for Escape to close a popover lose their place in the list.

**CONFIDENCE** certain

**FIX SKETCH** Add `bool DismissTopmostTransient()` to the overlay (closes the popover and returns `true` if one was open) and call it before `Hide(true)` in the ESC chain.

**Would an existing test have caught it?** No; the ESC dismiss order is only string-pinned, not behaviourally tested.

---

## 9. MINOR — With 64 custom pedals, importing another does nothing, says nothing, and leaks the copied file

**WHERE** `NeuralAmpModeler/VoLumCustomOverlay.h:611-638`

**MECHANISM** The in-loop error is unconditionally overwritten after the loop:

```615:619:NeuralAmpModeler/VoLumCustomOverlay.h
        if (volum::custom::AddPedal(base, rel) < 0)
        {
          mError = "Custom pedal slots are full - delete a pedal first.";
          continue;
        }
```
…then `:630-638` sets `mError` from `tooLarge` / `skipped` or, when both are empty, `mError.clear()`. Separately, `ImportFileCopy` (`:612`) has already copied the `.nam` into the library before `AddPedal` fails, and nothing rolls it back — one orphaned file per rejected import.

**TRIGGER** Fill the pedal library (64 entries), then Manage custom pedals → Import pedal (.nam) → pick a file. Nothing appears; no message.

**CONFIDENCE** certain

**FIX SKETCH** Only clear `mError` when the loop didn't set it (e.g. track a `bool slotsFull`), and call `store.RemoveStoredFile(rel)` on the `AddPedal(...) < 0` path.

**Would an existing test have caught it?** No; `test_volum_custom_content.cpp` covers `AddPedal` returning `-1` at the API level, not the overlay's error reporting.

---

## 10. MINOR — `>` on the preset bar with nothing selected recalls preset 2, not preset 1

**WHERE** `NeuralAmpModeler/VoLumPresetBar.h:195-213`

**MECHANISM** `const int idx = ((mIdx < 0 ? 0 : mIdx) + dir % n + n) % n;` — with no active preset (`mIdx == -1`) the "no selection" state is treated as index 0 and then stepped, so `>` lands on 1 and `<` lands on `n-1`. (The `dir % n` precedence slip is harmless: it equals `dir` for `n > 1` and yields 0 for `n == 1`.)

**IMPACT** From a fresh "No Preset" state, the first `>` skips the first preset.

**CONFIDENCE** certain

**FIX SKETCH** `const int idx = (mIdx < 0) ? (dir > 0 ? 0 : n - 1) : ((mIdx + dir + n) % n);`

**Would an existing test have caught it?** No; `Step` is private and untested.

---

## 11. MINOR — A freshly imported IR is selected but never scrolled into view

**WHERE** `NeuralAmpModeler/VoLumCustomOverlay.h:624-629` — `mSel = (int)mItems.size() - 1;` with no scroll adjustment, while the Builder's equivalent does exactly that (`mBuilderFileScroll = 1e9f;` at `:858`, "jump to the newly appended rows"). With ≥9 entries the newly imported IR and its selection dot are below the fold, so the import looks like it did nothing. **FIX**: set `mManageScroll = 1e9f` after a successful import (Draw already clamps). **No existing test**: import is only covered at the store level.

---

## NITs (one line each)

- `NeuralAmpModeler/VoLumCustomOverlay.h:269-277`: double-clicking a popover value field calls `OnMouseDown` twice, so `StartIrValueEntry` runs while an entry is already live — the first `CreateTextEntry` is abandoned without completion; harmless only because both entries share `mTextTarget`.
- `:1101-1111` `FmtIrCut` prints `%.1f kHz`, so a typed `2550` displays `2.6 kHz` and re-committing the prefill silently rewrites the value to 2600; same for `FmtIrTrim`'s `%+.1f`.
- `VoLumContentStore.h:258`: `strtod` accepts hex (`0x10` → 16) and `"1e999"` → `inf` → `Invalid` (silently ignored rather than clamped to max, unlike `"1e9"`).
- While a text entry is open, the first outside click is consumed by `ITextEntryControl` (commit), so dismissing the popover then takes two clicks.
- `:221-232` `OnMouseWheel` accumulates `mManageScroll` unclamped while the list is empty (Draw only clamps in the non-empty branch); harmless because the first populated draw clamps.
- `VoLumPresetBar.h:63-65` `SelectName` has no `break`, so with duplicate names it selects the last match.
- `VoLumAmpList.h:611` `mCustomArts[c] % kNumCustomArts` would index `mCustomArtLayers` negatively for a negative art id; unreachable today because the loader double-mods it (`VoLumContentStore.h:468`), so this is defence-in-depth only.
- `VoLumCustomOverlay.h:1352` calls `IRFileAt(i)`/`PedalFileAt(i)` per visible row per frame, each returning a fresh `std::string` — pure allocation churn in Draw.
- `VoLumListMenu.h:188-195` scrolls the menu on a wheel event anywhere in the window, not just over `mMenuRect`.

---

**Things I checked and found clean**, so they don't need re-auditing: nothing mutates the content store from a `Draw` call, and nothing draws from a mutation callback; the "only clicks outside dismiss" fix correctly swallows the dismissing click so no row behind the popover is hit; `mPopupIrIdx`, `mPopupFileIdx`, `mSel` and the confirm-dialog lambdas are all bounds-checked at use, and iPlug2's modality (`GetMouseControl` returns the text-entry control during an edit, and skips hidden controls) closes the "delete the item the popover is editing" and "commit after the panel closed" paths; list order is registry order with no sorting, so no cached-index-after-resort bug; `_VolumMigrateIrTrims` never reorders or removes entries; the greyed-out stepper states are derived from the same ladders/clamps the steppers use, and typed out-of-range values clamp before the availability check, so the two can't disagree; scrollbar geometry is shared between draw and drag hit-testing and is safe at zero/one/thousands of items and past both ends; and `VoLumAmpList::OnRescale` correctly drops both cached layer sets.

I've confirmed that `atof()` exhibits the same decimal-comma issue—typing "3,5" into the exact-value knob silently truncates to 3—so I'll add this as a related finding under the locale/comma category in my report, specifically noting VoLumExactEntry. Now I'm compiling the report with precise line numbers and code references, starting with the overlay action enum definitions.

Collecting all the specific line ranges for each issue... So the gear icons for rows 200+ also don't trigger anything since HandleManageAction doesn't process those codes. For rows 256-399, the body codes fall outside the valid range and don't select anything, and double-click also fails to trigger. But rows 400-499 are problematic—their codes map to the overwrite branch, which calls mOverwritePreset on the active amp's preset instead of the intended item, which is clearly wrong.

The real issue is that the action code spacing (100 apart per row) doesn't match the unbounded row count, violating the Builder's own documented invariant that each span must stay under 100. This is the root cause of the data loss vulnerability.

For fixes: either space the row action bases much further apart (1000+ or 4096), or better yet, use a single row-scoped hotspot record that maps a rect to an action and index pair instead of encoding the index into the action integer itself. The safest minimal change would be increasing the base spacing or adding an index field to the hotspot structure—no audio impact either way. Also worth adding the Builder's `rowVisible` guard to the Manage row loop before registering hotspots, and skipping icon registration for hidden rows. On the parsing side, accepting comma as a decimal separator would align with regional norms.

For the gear/shaping detection: track whether the user explicitly shaped the IR by adding a flag that's set only when SetIRShaping is called from the popover (not during migration, which writes fields directly), and treat trim as "shaped" only when cuts are active or the trim differs from the auto-normalized baseline. This avoids false positives without audio changes, though the flag won't persist across saves unless serialized.

For the sidebar desync issue: when deleting an amp, only reset the hero/name/selection if it was the focused one; otherwise shift the selection to the new index at that position.

For the trim step adjustment: apply the same adjacent-rung logic...

Clamping to keep the 0.5 grid without skipping; audio only changes where a step lands after a user types a value, leaving voicing untouched. Empty or whitespace-only commits count as "no change" (Invalid) across all fields, keeping "off"/"none"/"-" as Off only for cuts—for Level, only "off"/"none"/"0" resets to 0 dB while empty cancels. ESC should close just the popover by calling a dismiss handler before hiding. For pedal-full, avoid clearing mError when it's already set by the loop.

Preset bar cycling uses modulo arithmetic to wrap the index based on direction. Voicing stays frozen throughout—none of these fixes touch DSP except the stepper landing change which is user-initiated, and the flag in #4 must not alter trimDb. I should note for each finding whether an existing test would have caught it.