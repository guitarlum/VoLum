## 1. The gesture guard rejects every legitimate Manage-row double-click
**Severity** HIGH

**Where** `NeuralAmpModeler/VoLumCustomOverlay.h:198-202, 239, 289-303`

**What** `OnMouseDown` arms `mOwnsGesture`, but `OnMouseUp` unconditionally clears it. The comment at lines 291-293 states the Windows sequence itself: down, up, double-click, up. Therefore the flag is false when the legitimate `OnMouseDblClick` arrives, and lines 302-303 reject it. The guard blocks the primary row action it was meant to protect.

**How a user reaches it** Open Manage presets, Manage custom IRs, or Manage custom pedals; double-click a row. The first down selects the row, the first up clears `mOwnsGesture`, and the following double-click returns at line 303 without recalling the preset, selecting the IR, or loading the pedal.

**Fix** Do not use generic mouse-up ownership for this. Arm a pending double-click only when the preceding overlay mouse-down hit a row body (not an overwrite/rename/delete/gear hotspot), remember that row's stable id and click position, and consume/clear that arm in `OnMouseDblClick`. A double-click that began on the confirmation dialog then has no row-body arm, while a real row double-click remains usable.

## 2. Stable ids are sampled from the live list after the displayed rows have gone stale
**Severity** HIGH

**Where** `NeuralAmpModeler/VoLumCustomOverlay.h:538-548, 556-573, 887-895, 928-947, 1456-1526`

**What** `ReloadList` snapshots only names into `mItems`. The visible row and its action code keep that snapshot, but `RowIdAt(idx)` later reads the current process-global registry. If another editor removes an earlier item between drawing and clicking, the displayed row still names item B while `RowIdAt` at the same index returns item C's id. The confirmation names B and the identity-based callback faithfully deletes or overwrites C. The new id guard therefore protects changes after the prompt opens, but not the equally important interval between list rendering and the initiating click.

**How a user reaches it** Open the same IR, pedal, or preset library in two plugin editors. In editor A leave B visible at row 2. In editor B delete row 1. Without refreshing A, click B's trash icon in A and confirm. A takes the id now at row 2 (the former row 3), so the dialog says B but deletes that different item. The overwrite icon has the same failure.

**Fix** Snapshot ids alongside names in `ReloadList` (for example `mItemIds`, exactly parallel to `mItems`) and take the initiating id from that snapshot. Resolve that id immediately before the mutation. If the snapshotted id is gone, report that the named item disappeared; never fall back to the current occupant of its old row.

## 3. Preset identity is resolved before this editor claims its preset bank
**Severity** HIGH

**Where** `NeuralAmpModeler/VoLumCustomOverlay.h:538-545, 566-573, 593-615, 895-906, 937-947`; `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:1262-1265`

**What** Preset lists and id lookup use the process-global `ActivePresetOwnerKey`, but the overlay's claim callback runs only inside `ApplyRename`/`ApplyDelete`, after `RowIndexById`, or inside `_VolumOverwritePreset`, after the overlay has already converted the id to an index. With two editors, lookup can therefore run against editor B's bank and then the late claim switches to editor A's bank before the same numeric index is mutated. This can delete, rename, or overwrite an unrelated preset in A.

**How a user reaches it** Open editors A and B on amps with different preset banks. Let B be the last instance to perform a preset operation, then open Manage presets in A. A's `ReloadList` can show B's bank. Click B's row 1 trash icon and confirm. `RowIndexById` resolves row 1 in B; `ApplyDelete` then claims A and calls `DeletePreset` with row 1 in A, deleting A's unrelated preset at that index. Rename and overwrite follow the same ordering.

**Fix** Claim this editor's preset operations before every preset-bank read as well as every write: before `ReloadList`, `NameTaken`, the initial id snapshot, and each `RowIndexById`. Keep claim, resolve, and mutation in one synchronous operation. Prefer passing/capturing the explicit owner key so identity resolution is not dependent on mutable process-global context.

## 4. The IR shaping popover still tracks an IR by mutable row index
**Severity** MEDIUM

**Where** `NeuralAmpModeler/VoLumCustomOverlay.h:1100-1105, 1177-1201, 1211-1227`

**What** The popover stores `mPopupIrIdx` and uses it for every later step and text-entry completion. Unlike rename/delete/overwrite, it stores no id. A library deletion before that index retargets the still-open popover to the next IR, so a level/cut edit is persisted to the wrong item.

**How a user reaches it** In editor A open the shaping gear for IR B. In editor B delete an IR above B. Back in A click `+` on Level, or type a Low cut value and press Enter. `IRShapingAt(mPopupIrIdx)` and `SetIRShaping(mPopupIrIdx, ...)` now operate on the IR that shifted into B's old row.

**Fix** Store `mPopupIrId` when opening the popover and resolve it immediately before draw, step, and text completion. Close the popover with a “no longer in your library” message when the id no longer resolves.

## 5. A shifted custom-support index aliases a different amp
**Severity** MEDIUM

**Where** `NeuralAmpModeler/VoLumAmpMenus.inc.cpp:340-357, 391-400`

**What** `_VolumHasSupportAmp` only checks that `mVolumCustomSupportIdx` remains in range. If an earlier custom amp is deleted, the stale index can remain in range while referring to a different amp. `_VolumRefreshSupportChannels` then reads that different amp with `CustomAmpAt(mVolumCustomSupportIdx)`, so the support lane silently changes identity instead of becoming unresolved.

**How a user reaches it** Create custom amps A, B, and C; choose B as the support amp in editor 1 (index 1). In editor 2 delete A. B shifts to index 0 and C shifts to index 1. Return to editor 1 and focus/refresh Support: `_VolumHasSupportAmp` accepts index 1 and the lane displays/loads C.

**Fix** Treat the persisted `supportCustomId` as authoritative. Re-resolve the index from that id whenever the library changes and before testing/using the lane; if the id is absent, clear the runtime index and clamp focus to Main. A bounds check alone is not identity validation.

## 6. Support focus is clamped after the amp-knob group was already chosen
**Severity** MEDIUM

**Where** `NeuralAmpModeler/VoLumLayoutRuntime.inc.cpp:65-74, 192-206`

**What** The AMP branch reads `mVolumDualAmpFocusedSupport` and unhides either `SUPPORT_AMP_KNOBS` or `AMP_KNOBS` at lines 69-74. `_VolumClampSupportFocus` does not run until line 198. When it flips focus to Main, the later code repairs lane toggles and the cab row but never corrects the already-unhidden knob group, leaving Support knobs on a Main-focused view until another layout pass.

**How a user reaches it** Enable Dual Amp, focus Support, open the support-amp menu, and choose `(none)`. The resulting layout pass first exposes Support knobs, then clamps focus to Main. The hero/cab state says Main while the visible knobs still edit Support.

**Fix** Call `_VolumClampSupportFocus()` before the switch at line 65, before any visibility decision reads the focus flag. Then remove or retain only an idempotent defensive call later; do not rely on a second layout pass.

## 7. Clearing a hidden knob selection consumes Enter and the on/off shortcut
**Severity** MEDIUM

**Where** `NeuralAmpModeler/VoLumKeyboard.inc.cpp:37-49, 440-466`; `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:1420-1424`

**What** `_HandleVoLumKeyboardFocusKey` handles Return and B/Space only after returning early when a knob is selected. The following selected-knob handler detects that the selected control became hidden, clears the selection, and returns `true` for every key. The arrow-key consumption is intentional, but Return and B/Space are also swallowed even though they are the keys needed to enter the replacement knob or toggle the focused block.

**How a user reaches it** Select Delay TIME with Enter, click Sync so TIME is hidden and DIVISION replaces it, then press Enter: the first press only clears the stale TIME selection. Likewise, after hiding a selected knob, the first B in the plugin (or Space in standalone) does not toggle the focused effect.

**Fix** After clearing a hidden selection, consume only the dangerous arrow keys. For Return and B/Space, immediately re-run `_HandleVoLumKeyboardFocusKey(key)` now that no knob is selected (or clear stale selections when the mode changes), so the user's key performs its advertised action once.

## 8. A malformed exponent is accepted as a different valid number
**Severity** LOW

**Where** `NeuralAmpModeler/VoLumNumericEntry.h:62-79`

**What** `strtod("1e", &stop)` parses `1` and leaves `stop` at `e`; the suffix validator then accepts `e` as a unit letter. A partially typed exponent is therefore committed as 1 instead of being rejected like other malformed numbers. The same permissive suffix rule accepts arbitrary alphabetic junk such as `1oops`.

**How a user reaches it** Open an exact-value entry, type `1e`, and press Enter. The knob moves to 1 (or its constrained equivalent) even though the entered exponent is incomplete.

**Fix** Parse a complete decimal/scientific numeric token first, requiring exponent digits when `e`/`E` is present, then validate only an explicit unit suffix. At minimum, reject `e`/`E` at `stop` when it could be the beginning of an incomplete exponent.

## 9. Action families collide again at row 65,536
**Severity** LOW

**Where** `NeuralAmpModeler/VoLumOverlayActionCodes.h:22-24, 44-56`; `NeuralAmpModeler/VoLumCustomOverlay.h:431-439, 841-864, 1048-1065`

**What** `kActionStride` is 65,536, but no list or builder manifest is capped below that size. Family decoding still uses half-open ranges of exactly one stride, so `ActionCode(family, 65536)` is exactly `ActionBase(nextFamily)`. For example, the speaker hotspot for builder file 65,536 decodes as the channel hotspot for file 0.

**How a user reaches it** Load or build a library/manifest with 65,537 rows, scroll to row 65,536, and click a row action. The encoded action belongs to the next family and runs that family's row-0 action instead.

**Fix** Enforce a hard maximum of `kActionStride` rows before constructing hotspots, or stop encoding family and index into a single unchecked `int` (store a typed `{family, index}` action). Add a boundary test for index `kActionStride`, not only `kActionStride - 1`.
