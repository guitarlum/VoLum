---
name: a2-training
description: Retrain VoLum bundled NAM captures to Architecture 2 on RunPod. Use when retraining rigs, packing SlimmableContainer A2 models, running the cloud training fleet, discussing A2-Lite vs Full, or repeating a capture training round.
---

# A2 Training

Operational how-to: `training/cloud/README.md`. This skill is the **why** — locked
recipe, measured traps, and how VoLum loads the result. Do not re-discover it.

## Recipe (locked)

`nam.train.core.train(...)` (neural-amp-modeler 0.13.0). Do not hand-roll data
configs. It auto-detects the standard v3 input, auto-calibrates **per-file
latency**, then trains.

- **A2 packed export:** `to.model.net.export(Path(OUT), basename=name)` writes
  architecture `SlimmableContainer` with `channels_3` (Lite, max 0.5) +
  `channels_8` (Full, max 1.0).
- `batch_size=16`. Measured on AMP-2203-4 @ 700ep: 16 → Full ESR 0.0189; 64 →
  0.0334; 256 → 0.0698. Do not raise batch.
- `epochs=700` is an **upper bound**. Best-fit keeps the best checkpoint
  (`packed_best.json`). Hard amps converge ~468–591; easy ones by ~200. Cutting
  to 400 hurts hard amps; >700 is wasted GPU.
- Escalation **off** (`escAt=100000`). Auto-extending epochs chased 6th-decimal
  ESR noise.
- `ignore_checks=True` — these recordings already made good A1 models.
- QA Full on **channels_8** ESR, not `validation_esr` (that is Lite+Full sum,
  Lite-dominated). Inspect with `training/cloud/check_nam.py`.

Input sweep is NAM's standard v3 (`T3K-sweep-v3.wav`, md5
`36cd1af62985c2fac3e654333e36431e`). Not in the repo. Upload beside captures.

## How VoLum plays it

NAMCore `ContainerModel` defaults to the **last** (largest) submodel. Packed A2
loads via `nam::get_dsp` with no plugin change and runs Full. Lite is the
Settings switch (`_VolumSetLiteMode` → `SetSlimmableSize`). A plain WaveNet
file has no Gateway Slim/Thin toggle; packed does. That is cosmetic, not a
training bug.

Map trained files **by basename**, not folder: 249 capture WAV names match 249
`rigs/**/*.nam` names exactly. Swap each trained `*.nam` from the a2-final
training output onto the matching `rigs/` leaf, then `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
(`test_nam_rigs.cpp`, `test_volum_pre_pedal_captures.cpp`) and a changelog line.

## Fleet lessons (paid for)

This workload is **validation/CPU-bound**, not GPU-FLOPS-bound. Cap threads
(`THREADS=2`, ~3 on a 32-vCPU 4090) and run **many tmux shards per GPU**.
Uncapped threads oversubscribe vCPUs and slow the run.

Best-of-N independent 700–1100ep runs beat grinding one run longer. Capture
quality caps ESR — the high-gain `Tri2-2` family flattens early; flag
re-capture, do not train longer.

RunPod 4090 Secure Cloud was ~$0.69/hr, not the community rate. Always
`Fleet-Teardown`. A laptop watcher dies if the machine sleeps; unattended runs
need a **pod-side** self-terminating job.

## PowerShell + SSH

Do not inline `$`, `$(...)`, or `$?` in an ssh command from PowerShell — they
expand on the laptop. `\$` does not save you. Scp a script, `sed -i 's/\r$//'`
it, then run it. `$ProgressPreference='SilentlyContinue'` for pip/conda.

Git remotes on this machine: explicit HTTPS (`.cursor/rules/git-remote-https.mdc`).

## Last full run

RunPod 2026-06-06/07: 249 captures, round-2 best-of-2 on ~30. Merged set was
`training/a2-final/` (gitignored). Pods and the network volume were deleted.
The A2 files in `rigs/` are the shipped result.
