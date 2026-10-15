# 1.3.0 scope

## Destination

A locked in/out list for the next public minor after 1.2.2, plus the decisions coding cannot start without. This map is not shipped 1.3.0. Specs and overnight conductor sessions start when no tickets remain.

## Notes

- Domain: VoLum product scope. Every session on this map: grilling + domain-modeling. HITL rounds use Cursor AskQuestion. Implementation skills stay off until the map is clear.
- **This map is planning.** One non-research ticket per session. Claim (`Status: claimed`) before work. Do not implement on this map.
- **HITL vs AFK.** Grilling tickets need the owner live — a sub-agent must not answer them. Research tickets are AFK and may be parallelized. Prototype tickets need the owner to react to the artifact.
- **After this map is clear** (no open tickets): a *conductor* session, not `/wayfinder`. First implementation on `dev`: AudioDSPTools Unicode WAV/IR paths (`#25`) per [Does a short upstream NAM-player sweep land before 1.3.0 feature work?](issues/15-upstream-before-features.md). Then `.scratch/<feature>/spec.md` plus `ready-for-agent` issues (one directory per headline). One main agent; sub-agents per spec. Tests are mandatory (Windows suite; MIDI decoder/handoff doctests; pack permutation tests; chorus DSP/UI/state pins). Present a UAT build (standalone + VST3) to the owner; do not promote to `main` without that UAT. Set the conductor timebox when that session starts — this book is larger than one night, so “1.3.0 UAT tomorrow” is not a standing promise.
- 1.3.0 is the next public minor. The WSOLA cheapening already on `dev` is in by default. `config.h` is still 1.2.2.
- Headlines: MIDI control, PLAY vs BUILD, a portable Pack of machine settings + custom content, POST Chorus.
- Gates that ride with those headlines (verified in code, not backlog prose): two-writer content library, delete-while-playing, forward-compatible DAW chunks before any new params, audio-thread MIDI handoff.
- GitHub Issues stay user-facing ([Preset setting and MIDI control](https://github.com/guitarlum/VoLum/issues/15), [Chorus effect](https://github.com/guitarlum/VoLum/issues/25)). Do not publish this map there.
- CI is GitHub Actions on this repo, not GitLab.
- Trust `NeuralAmpModeler/` over `backlog/*.md`. Backlog files are prior art to migrate, not evidence.
- Local chorus measurement notes stay off this repo. Tickets discuss VoLum voices only.
- Tracker: `.scratch/release-1.3.0/` (see `docs/agents/issue-tracker.md`).

## Decisions so far

- [How do we prove MIDI without a controller?](issues/02-midi-without-hardware.md): CI can prove the decoder and the audio→main handoff with no hardware; pluginval is not semantic proof; Windows standalone OS-MIDI still needs a loopback port.
- [What MIDI control does 1.3.0 include?](issues/01-midi-control-scope.md): VST3+AU+standalone, MIDI in only; Program Change recalls a machine-global sound (amp + named preset); Settings MIDI list; no Learn, no CC0/CC32.
- [Which chorus voice families fit VoLum POST?](issues/09-chorus-voice-families.md): research families Clear / Warm / Ensemble behind RATE DEPTH TONE WIDTH MIX; default Clear. Shipping pick is [Where does Chorus sit in POST, and which voices?](issues/08-chorus-placement-and-voices.md).
- [How does MIDI leave the audio thread?](issues/03-midi-audio-thread-handoff.md): MIDI-only latest-wins slot queue, drained in OnIdle; headless sound recall; B7 staging stays out.
- [How does the content library survive two writers?](issues/04-two-writer-library.md): lock+merge by stable id; custom amps = stock (live rig on the instance); MIDI map with the library; Pack import merges by id and applies to this instance only.
- [What happens when we delete content that is currently playing?](issues/05-delete-while-playing.md): this instance reverts in-use graph slots to an available default (not a ghost capture); preset delete keeps the sound; siblings stay until they next need the id; MIDI holes stay numbered and show invalid; Pack replace reloads after confirm. Tests are a gate.
- [What is in a Pack, and what happens on conflict?](issues/06-pack-contents-and-conflict.md): one `.volumpack`; Everything vs selection with auto-included companions; SHARE merges; FULL Overwrite/Add/Reset; settings restore is standalone-only; transactional; contract version.
- [How does a current build read a future DAW chunk?](issues/07-forward-compatible-chunks.md): 1.2.2 keeps the old rig (Chorus gone); freeze the 93-double prefix and binary per-amp tail; new instance state in id-tail JSON; EParams allowed but not extra prefix doubles; 1.3.0 reads 93 not live `kNumParams`, before any param bump.
- [Where does Chorus sit in POST, and which voices?](issues/08-chorus-placement-and-voices.md): Chorus → Delay → Reverb → Tremolo; fourth card; CLASSIC / WARPED / CLEAR / ENSEMBLE, default WARPED; RATE DEPTH TONE WIDTH MIX; no reorder in this book.
- [Is the update notifier in this 1.3.0 book?](issues/10-update-notifier.md): must-ship, not a headline; F14 contract except badge clears only on update row / Check now, not on opening Settings.
- [What is PLAY vs BUILD in 1.3.0?](issues/11-play-vs-build.md): all formats; PLAY = assigned Sounds in PC order + PRE/POST bypass + Add; BUILD = today’s editor; remember per instance. Layout mockup is [Does the PLAY surface fit at 900×600?](issues/12-play-surface-mockup.md).
- [Does the PLAY surface fit at 900×600?](issues/12-play-surface-mockup.md): yes. Opus D — centre breathing art, PC thumb rail, stomps on the bottom; rail scrolls, LIVE sticks, Add pinned. Dual Amp stays inside the Sound. Mock: `.scratch/release-1.3.0/play-proto/opus2/index.html?variant=D`.
- [Does 1.3.0 ship a factory preset bank?](issues/13-factory-preset-bank.md): in — one read-only Factory Preset per factory amp; Factory/User lists; PLAY Add is one Sound list; no MIDI seed.
- [What Chorus card motif ships with the four modes?](issues/14-chorus-card-motif.md): one Throat wormhole for all four modes; same drawing at Quiet 20 px, gold gated > 40 px; Quiet label stays CHORUS. Mock: `.scratch/release-1.3.0/chorus-motif/index.html`.
- [Does a short upstream NAM-player sweep land before 1.3.0 feature work?](issues/15-upstream-before-features.md): yes, but only AudioDSPTools `#25` (Unicode WAV/IR paths) before headlines; skip NAM Player plugin chrome; park NAMCore until after 1.3.0.

## Not yet specified

- None. The way is clear.

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
- A 1.2.3 that teaches the shipped reader to skip extra prefix doubles (1.2.2 is frozen; 1.3.0 writes a format that reader already skips).
- Auto-update / code signing (the [update notifier](issues/10-update-notifier.md) is notify-only).
- Marketplace of custom amps or factory-amp presets (SHARE is person-to-person Pack, not a store).
- POST effect reorder (later BUILD complexity if PLAY earns it; [Where does Chorus sit in POST, and which voices?](issues/08-chorus-placement-and-voices.md) locked a fixed Chorus → Delay → Reverb → Tremolo chain for this book).
- Independent PLAY setlist order (drag without changing Program Change) and drag-reorder that rewrites PC numbers ([What is PLAY vs BUILD in 1.3.0?](issues/11-play-vs-build.md) locked assigned slots in PC order, no drag).
- Per-mode Chorus motif variants (Pitch-style helix vs chevrons). [What Chorus card motif ships with the four modes?](issues/14-chorus-card-motif.md) locked one Throat for CLASSIC / WARPED / CLEAR / ENSEMBLE.
- NAMCore bump and the rest of R3 (35 commits including Eigen 5.0.1, Sequential, FFT convolution). [Does a short upstream NAM-player sweep land before 1.3.0 feature work?](issues/15-upstream-before-features.md) parked that until after 1.3.0; keep our two unique NAMCore commits when it happens.
- NAM Player plugin chrome since June (0.8.0 branding, file-browser error UI, mouse-capture OpenURL, their iPlug2 SHA). Same ticket: VoLum never attaches that UI and does not take their iPlug2 pointer.
