# F4 — Optional A2-Lite mode (run the slimmable container)

Plan an opt-in "Lite mode" so VoLum can run the smaller A2-Lite (channels_3)
slice of a NAM A2 model on low-powered machines, while keeping A2-Full as the
default, best-in-breed execution.

Background (see `docs/a2-training-runbook.md`): NAM A2 trains a single packed
`SlimmableContainer` `.nam` holding both A2-Full (channels_8) and A2-Lite
(channels_3). VoLum now ships these **packed containers**; the core
`ContainerModel` defaults `_active_index` to the LARGEST slice (Full), so VoLum
already plays Full with no plugin change (`NAM/container.cpp`). The plugin runtime
does NOT yet call `SetSlimmableSize`, so there is currently no way for a user to
opt into the Lite slice — that is exactly what this ticket adds. The core already
supports containers (`NAM/container.{h,cpp}`, `NAM/slimmable.h`,
`SlimmableModel::SetSlimmableSize`).

Decide and produce a feature ticket covering:
- The rigs are already packed containers (Full + Lite in one file), so no asset
  change is needed — this ticket is purely the runtime opt-in + UI.
- Where the Full/Lite choice lives: global setting vs per-rig. Default MUST be
  Full ("best in breed"). Lite is an explicit opt-in for CPU-constrained users.
- Plugin wiring: after `get_dsp`, detect `nam::SlimmableModel` (dynamic_cast)
  and call `SetSlimmableSize` to select Full (max) by default, Lite when the
  user opts in. Handle non-slimmable (plain WaveNet) rigs gracefully.
- UI affordance (where the toggle lives; how it reads/persists) and interaction
  with existing settings persistence in `VoLumSettings.inc.cpp` and the chunk
  version in `test_volum_chunk_version.cpp` / `test_volum_chunk_codec.cpp`.
- Reset/threading correctness: switching size must `Reset` the model safely off
  the audio thread (the container takes a `_slim_set_mutex`).
- Tests: slimmable rig load + size-switch smoke (extend the existing
  "Core slimmable NAM example loads and processes" case in `test_nam_rigs.cpp`),
  settings round-trip for the new toggle, and a CPU-usage sanity note.
- Docs EN/DE + changelog.

Acceptance: default playback is unchanged (A2-Full); enabling Lite measurably
lowers CPU; setting persists; tests/docs/changelog in place. Do not implement
in the planning chat.

Work on a dedicated feature branch off the latest `dev`, named
`feature/a2-lite-mode`. Merge back into `dev` only after acceptance criteria
are met. Never promote to `main` outside of a release.
