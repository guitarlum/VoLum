# Factory preset bank

Status: resolved
Blocked by: none

## Goal

One read-only Factory named Preset per factory amp, per [Does 1.3.0 ship a factory preset bank?](../../release-1.3.0/issues/13-factory-preset-bank.md).

## Do this

Ship snapshots as data next to the rigs (JSON of `VoLumAmpSettings` keyed by `factory:<idx>:v1`), not as content-library rows. Display name **Ready**. Shipped graph only (no custom amp/IR/pedal). BUILD menu subsections Factory / User; Default stays the pinned reset above both. Manage: User only. Dirty Factory → Save writes User copy.

PLAY Add can wait for ticket 02 but the Sound list helper should exist headlessly here.

## Tests (must fail with this ticket reverted)

15 Factory ids; custom amp has no Factory row; Manage delete/rename/overwrite refuse Factory ids; Save-on-Factory mints User and leaves Factory snapshot unchanged; upgrade can replace the snapshot file without changing the id.

## Done when

Windows suite. Revert-fail proven.
