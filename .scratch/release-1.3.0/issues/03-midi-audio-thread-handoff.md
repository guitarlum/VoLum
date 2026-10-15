# How does MIDI leave the audio thread?

Type: grilling
Status: resolved
Blocked by: 01

## Question

iPlug calls into MIDI on the high-priority audio thread. Amp / channel / preset selection today touches the content registry, filesystem-backed channel discovery, async model queues, and UI. Calling those directly from MIDI would violate the real-time contract. That is why the 1.2.1 spike was gated (`backlog/F9-midi-support.md`, `backlog/B7-audio-thread-rt-violations.md`).

Code still agrees: `ProcessBlock` takes blocking staging mutexes; there is no lock-free command queue to the main thread (`_ApplyDSPStaging`, loader mutex/deque).

Given the MIDI shape from [What MIDI control does 1.3.0 include?](01-midi-control-scope.md) — Program Change into a machine-global **sound** map (amp + named preset), MIDI channel filter, no CC0/CC32 — what is the smallest handoff 1.3.0 will ship? The queued command is “recall this sound,” not “set amp index / preset index on the current amp.”

- A bounded lock-free audio→main command queue used only by MIDI, or
- The shared handoff B7 wanted for model/UI work too?

Recommend: MIDI-only lock-free queue + one headless selection service shared by mouse, keyboard, and MIDI. Do not take the full B7 “every RT sin” pass in this minor (that remainder is out of scope on the map).

## Answer

**1.3.0 ships MIDI→main only.** Program Change is parked on the audio thread and the Sound is recalled on the normal thread. Amp/IR swapping inside `ProcessBlock` (`_ApplyDSPStaging` mutexes, loader drain) is unchanged. That rewrite is B7 and stays out of this minor — opposite pipe (models arriving *to* the callback, not MIDI leaving it).

**Audio thread.** Channel filter, then read Program Change. Enqueue the slot `0–127`. No registry, no MIDI sound map, no filesystem, no UI.

**Queue.** Capacity 1, latest-wins overwrite. A stomp burst sounds the last slot, not a backlog of NAM loads. A Program Change while a load is in flight replaces the pending recall.

**Drain.** `OnIdle`, same as today’s model load. Apply with the editor closed. If chrome exists, refresh it; if not, set the existing UI-sync pending flag so a later open matches what is sounding.

**Shared recall, not shared queue.** Mouse and keyboard stay on the UI thread and do not enqueue. Extract a headless sound-recall path (stable amp identity + named preset) that the drain calls. Sidebar / preset-bar callbacks keep their glue but must use the same apply guts so custom-amp cabs cannot diverge. `GetUI()` is paint-only.

Unassigned, out of range, or missing amp/preset stay **ignore**, per [What MIDI control does 1.3.0 include?](01-midi-control-scope.md). CI proof stays [How do we prove MIDI without a controller?](02-midi-without-hardware.md): decoder + queue + headless recall; not pluginval.
