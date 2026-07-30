# F9 — MIDI support (lowest priority)

Plan minimal, opt-in MIDI support. One user requested it; it is lowest priority and intentionally low-UI. Scope: MIDI Program Change recalls presets (F5); MIDI CC maps to a curated set of continuous params (e.g. amp / PRE NAM / support-amp / COMP levels, reverb/delay mix) via MIDI-learn; optional MIDI channel filter. Keep the UI unobtrusive: a MIDI-learn affordance plus a small mappings list inside the existing settings overlay (`VoLumSettingsOverlay.h`) — no always-visible clutter in the main view. Decide: which params are mappable and their value scaling; MIDI map persistence (user settings JSON); standalone MIDI input handling and device picker (iPlug2 standalone MIDI capabilities) versus the plugin receiving MIDI from the host (plugin MIDI-in flag in `config.h`); and Program-Change -> preset-index resolution (depends on F5's preset list/ordering). Produce a feature ticket with: mapping model + mappable-param list, MIDI-learn UX spec, persistence schema, standalone vs plugin input strategy, tests (CC -> param mapping as a pure helper, program-change -> preset index, persistence round-trip), docs EN/DE + changelog. Do not implement.

This ticket depends on F5 (Program Change -> preset). Work must happen on a dedicated feature branch off the latest `dev`, named `feature/midi-support`. Do not commit to `dev` or `main` directly. The branch is merged back into `dev` only after the ticket's acceptance criteria are met and tests/docs/changelog are in place. Never promote to `main` outside of a release.

## 1.2.1 spike result (deferred)

The patch-release spike was gated out. iPlug calls `ProcessMidiMsg()` on the high-priority audio thread, while amp/channel/preset selection currently touches the content registry, filesystem-backed channel discovery, async model queues, and UI controls. Calling those paths directly from MIDI would violate the real-time contract. A safe implementation needs:

- a pure, per-MIDI-channel Bank Select decoder (`CC0` amp, `CC32` channel, then Program Change preset);
- a bounded lock-free command handoff from `ProcessMidiMsg()` to the main/idle thread;
- one headless selection service shared by mouse, keyboard, and MIDI (custom-amp channel resolution currently depends on attached UI controls);
- explicit zero-based factory/custom amp, channel, and preset bounds;
- APP MIDI-device selection plus VST3/AU host-routing and pluginval/manual validation.

That is feature work, not a low-risk 1.2.1 patch. Revisit for 1.3.0 on `feature/midi-support`; keep arbitrary CC mapping/MIDI Learn and mapping persistence as a later layer after deterministic Bank Select + Program Change works cross-format.
