# 1.3.0 scope

## Destination

A locked in/out list for the next public minor after 1.2.2, plus the decisions coding cannot start without. This map is not shipped 1.3.0. Specs and overnight conductor sessions start when no tickets remain.

## Notes

- Domain: VoLum product scope. Every session on this map: grilling + domain-modeling. Implementation skills stay off until the map is clear.
- **This map is planning.** One non-research ticket per session. Claim (`Status: claimed`) before work. Do not implement on this map.
- **HITL vs AFK.** Grilling tickets need the owner live — a sub-agent must not answer them. Research tickets are AFK and may be parallelized. Prototype tickets need the owner to react to the artifact.
- **After this map is clear** (no open tickets): a *conductor* session, not `/wayfinder`. That session writes `.scratch/<feature>/spec.md` plus `ready-for-agent` implementation issues, then one main agent spawns sub-agents per spec. Tests are mandatory (Windows suite; MIDI decoder/handoff doctests; pack permutation tests; chorus DSP/UI/state pins). Present a UAT build (standalone + VST3) to the owner; do not promote to `main` without that UAT. Set the conductor timebox when the map closes — this book is larger than one night, so “1.3.0 UAT tomorrow” is not a standing promise.
- 1.3.0 is the next public minor. The WSOLA cheapening already on `dev` is in by default. `config.h` is still 1.2.2.
- Headlines: MIDI control, a portable Pack of machine settings + custom content, POST Chorus.
- Gates that ride with those headlines (verified in code, not backlog prose): two-writer content library, delete-while-playing, forward-compatible DAW chunks before any new params, audio-thread MIDI handoff.
- GitHub Issues stay user-facing ([Preset setting and MIDI control](https://github.com/guitarlum/VoLum/issues/15), [Chorus effect](https://github.com/guitarlum/VoLum/issues/25)). Do not publish this map there.
- CI is GitHub Actions on this repo, not GitLab.
- Trust `NeuralAmpModeler/` over `backlog/*.md`. Backlog files are prior art to migrate, not evidence.
- Local chorus measurement notes stay off this repo. Tickets discuss VoLum voices only.
- Tracker: `.scratch/release-1.3.0/` (see `docs/agents/issue-tracker.md`).

## Decisions so far

- [How do we prove MIDI without a controller?](issues/02-midi-without-hardware.md): CI can prove the decoder and the audio→main handoff with no hardware; pluginval is not semantic proof; Windows standalone OS-MIDI still needs a loopback port.
- [Which chorus voice families fit VoLum POST?](issues/09-chorus-voice-families.md): Clear / Warm / Ensemble behind RATE DEPTH TONE WIDTH MIX; default Clear.

## Not yet specified

- Factory preset bank (cold-start demos; export of a user preset folds into Pack).
- Chorus card motif / ASCII art, once voices exist.
- Pack file extension and whether v1 offers subset exports or only “everything”.
- How far past the MIDI handoff the audio-thread work should go in this minor.
- Whether a short upstream NAM-player sweep lands before feature work.

## Out of scope

- Linux ([GitHub #17](https://github.com/guitarlum/VoLum/issues/17)).
- CLAP ([GitHub #20](https://github.com/guitarlum/VoLum/issues/20)).
- In-app Tone3000 browser ([GitHub #26](https://github.com/guitarlum/VoLum/issues/26)).
- Custom pedal type groups (`backlog/F8-import-your-own-pedals.md`).
- Custom-amp artwork reroll (`backlog/F13-custom-amp-artwork-reroll.md`).
- Octaver voicing research pass (`backlog/F11-octaver-deep-research.md`).
- Tremolo voicing research pass (`backlog/F12-tremolo-deep-research.md`).
- Manage/Builder overlay split (`backlog/Q2-volum-1.2.0-structure-decompose.md` phase 3).
- Installer PDB / symbol mismatch (`backlog/B8-pdb-symbol-mismatch.md`).
- Full audio-thread hardening beyond the MIDI handoff (`backlog/B7-audio-thread-rt-violations.md` remainder).
- Auto-update / code signing (notifier, if in, is notify-only).
