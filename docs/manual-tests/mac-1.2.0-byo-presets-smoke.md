# macOS 1.2.0 BYO + Presets Smoke Test

Minimal manual pass for the 1.2.0 backend (F5 presets, F6 custom amps, F7 custom
IR, F8 custom pedals) on a Mac. Owner: tester on a Mac. Estimated runtime:
~20-30 minutes.

Automation already covers the content-store registry, file copy, id resolution,
preset capture/recall/dirty, chunk round-trip, and version migration (doctests
in `NeuralAmpModeler/tests/`). This list only covers what cannot run in CI:
real macOS file permissions in the **sandboxed AU/AUv3**, and that all three
formats truly share one library. Keep it to these must-dos; skip permutations.

## Pre-flight

- [ ] Clean Mac or fresh user account (no carried-over VoLum content/prefs).
- [ ] Have 2-3 of your own `.nam` files and one IR `.wav` ready to import.
- [ ] Note macOS version + audio interface.
- [ ] Content library lives at
      `~/Library/Application Support/VoLum/content` with `volum-content.json`
      and `amps/`, `ir/`, `pedals/` subfolders. Confirm it does NOT exist yet.

## Standalone: import + play (F6/F7/F8)

1. [ ] Launch the standalone. In the amp browser CUSTOM section, press **+**,
       name an amp, add your `.nam` file(s), assign cab+channel, Save.
2. [ ] Select the new custom amp. It must LOAD and PRODUCE SOUND (not silent,
       no NaN mute). Switch cabs/channels you defined.
3. [ ] Confirm `~/Library/Application Support/VoLum/content/amps/<id>/` now
       holds a COPY of your `.nam` (originals can be moved/deleted afterwards
       and the amp still loads on relaunch).
4. [ ] Speaker row -> **Custom IR** cab -> import your `.wav`. Confirm it
       convolves (tone changes) and the file was copied into `content/ir/`.
5. [ ] PRE NAM dropdown -> CUSTOM -> import a `.nam` pedal capture; load it into
       a PRE slot and confirm it processes. File copied into `content/pedals/`.
6. [ ] Dual Amp -> SUPPORT -> CUSTOM -> pick your custom amp. Confirm the
       SUPPORT lane produces sound (custom partner actually loads its `.nam`).

## Standalone: presets (F5)

7. [ ] On a custom amp, dial a tone, preset bar -> **Save current as new**.
8. [ ] Change a knob -> bar shows **(unsaved)**. Recall the preset -> sound and
       knobs restore exactly and **(unsaved)** clears.
9. [ ] Set the knob back to the saved value by hand -> **(unsaved)** clears on
       its own (equality-based dirty state).
10. [ ] **Update** the preset, **Rename**, then **Delete** it; list updates.
11. [ ] Quit and relaunch standalone: custom amps, IR, pedals, and presets are
        all still present (read back from the shared library).

## AU / AUv3 sandbox (the headline mac-only risk)

The AU runs sandboxed; this verifies VoLum can read AND write the content
library from inside the sandbox container.

12. [ ] Logic Pro: insert VoLum (AU). Open a custom amp created in the
        standalone -> it must load and play (sandbox READ of the shared
        library works).
13. [ ] From inside Logic's AU, create a NEW custom amp / import a NEW IR.
        Expectation: import succeeds, the file is copied, and the new item
        appears (sandbox WRITE works). If a macOS file-access prompt appears,
        grant it.
14. [ ] Open GarageBand or Logic and load VoLum as **AUv3**. Confirm the SAME
        custom content created above is visible (one shared library across
        APP / AU / AUv3), and a custom amp loads + plays.
15. [ ] `auval -v aufx VoLm Lum0` passes with no new errors.

## DAW project round-trip (chunk 1.2.0 id tail)

16. [ ] In Logic/Reaper, on a custom amp, recall a preset, pick a custom
        SUPPORT amp, select a Custom IR. Save the project. Close. Reopen.
17. [ ] Expectation: the same custom MAIN amp is focused, the custom SUPPORT
        partner is restored and audible, the Custom IR is active, and the
        active preset label is shown clean (no **(unsaved)**).
18. [ ] Backwards-compat: open a project saved with **1.1.0** (no custom
        content). Expectation: it loads unchanged - no errors, factory amp
        state intact (the 1.2.0 id tail is simply absent).

## Failure handling

If any check fails, note the exact step, build version (VoLum -> About), macOS
version, whether it was APP/AU/AUv3, and a 1-line repro.
