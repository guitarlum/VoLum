# How does MIDI leave the audio thread?

Type: grilling
Status: open
Blocked by: 01

## Question

iPlug calls into MIDI on the high-priority audio thread. Amp / channel / preset selection today touches the content registry, filesystem-backed channel discovery, async model queues, and UI. Calling those directly from MIDI would violate the real-time contract. That is why the 1.2.1 spike was gated (`backlog/F9-midi-support.md`, `backlog/B7-audio-thread-rt-violations.md`).

Code still agrees: `ProcessBlock` takes blocking staging mutexes; there is no lock-free command queue to the main thread (`_ApplyDSPStaging`, loader mutex/deque).

Given the MIDI shape from [What MIDI control does 1.3.0 include?](01-midi-control-scope.md), what is the smallest handoff 1.3.0 will ship?

- A bounded lock-free audio→main command queue used only by MIDI, or
- The shared handoff B7 wanted for model/UI work too?

Recommend: MIDI-only lock-free queue + one headless selection service shared by mouse, keyboard, and MIDI. Do not take the full B7 “every RT sin” pass in this minor (that remainder is out of scope on the map).
