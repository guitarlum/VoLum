# A2 Training Runbook (RunPod, handover)

How VoLum's bundled captures were retrained from NAM A1 to **Architecture 2
(A2)** on rented **RunPod** GPUs, written so a future session (or a fresh agent
after context compression) can repeat a round without re-discovering everything.

- **The why** (design decisions, measured numbers, lessons) is this doc.
- **The how** (what each script does, how to run a round) is the operational
  toolkit in [`training/cloud/`](../training/cloud/) — start with its
  [`README.md`](../training/cloud/README.md).

> We originally planned to drive a local RTX 4090 PC over SSH from WSL2. We
> pivoted to RunPod: faster, fully parallel, and it didn't tie up a PC for days.
> The local-PC scripts were retired; the **A2 recipe, the packed-container
> decision, and the basename mapping below apply regardless of where the GPU
> runs.**

## TL;DR of the approach

- GPU work can't run over the network. We **rent GPUs** (RunPod — many `tmux`
  shards across a few 4090s sharing one network volume).
- The laptop's Cursor session only **orchestrates**: provisions/SSHes into the
  pods, launches sharded training jobs, polls progress, pulls results back, and
  tears the pods down. All PyTorch/CUDA + the NAM trainer run on the pod.

## Learnings from the cloud run (RunPod, 2026-06-06/07)

The full 249-capture run was done on RunPod, then a targeted round 2 retrained a
minority at higher epochs. Toolkit + sketch: [`training/cloud/`](../training/cloud/).

**Workflow that worked**

- A few pods (round 1: 1× RTX 4000 Ada + 3× RTX 4090) sharing one **network
  volume** (`/workspace`, holds captures + outputs). Captures uploaded once;
  every pod reads the same `captures_full/` and writes to a shared `out/`.
- Each pod runs many detached `tmux` shards via `launch_shards.sh`; shards
  partition `0..NSHARD-1` (`SHARD % NSHARD`). `batch_train_cloud.py` skips any
  capture whose `.nam` already exists, so the whole run is resumable and a dead
  pod just leaves work for the others.
- Pull results with `Pod-ScpFrom`; merge `results_*.csv`; **always**
  `Fleet-Teardown` (delete pods) when done.

**Lessons (the expensive ones)**

- **This workload is validation/CPU-bound, not GPU-FLOPS-bound.** A 4090 sits
  far from saturated; the bottleneck is per-epoch validation on CPU. So pack
  **many shards per GPU** and **cap CPU threads per shard** (`THREADS=2`, ~3 on
  a 32-vCPU 4090). Uncapped threads oversubscribe the vCPUs (load avg 13+ on a
  9-vCPU box) and *slow everything down*. This single fix was the biggest
  throughput win.
- **Epoch escalation was a mistake — turn it off.** The "retrain longer if
  `best_epoch` is near the cap" net chased 6th-decimal ESR noise and burned
  GPU hours for inaudible/no gains. Run a **flat 700** (best-fit already keeps
  the best checkpoint, so 700 is a safe upper bound). Launch with
  `escAt=100000` to disable.
- **Run-to-run variance is real; best-of-N beats more epochs.** For the genuine
  under-fit minority, a second independent 700–1100-epoch run often produced a
  lower-ESR model than grinding one run longer (random init lottery). Round 2
  retrained ~30 candidates and we kept the **lower-ESR `.nam` per capture**
  across rounds → `training/a2-final/` (provenance in `round_provenance.csv`).
- **Capture quality caps ESR — more training can't fix a noisy capture.** The
  worst residual ESRs (the `Tri2-2`/high-gain family) are capture-limited, not
  under-trained: their curves flatten early and round 2 barely moved them. Flag
  these as "re-capture if you care", not "train longer".
- **A local watcher that tears pods down will fail if the laptop sleeps.** We
  lost pod-hours twice this way. The watcher (`monitor_loop.ps1` / `r2_watch.ps1`)
  is fine for *attended* runs, but for unattended runs put the teardown
  **on the pod** (a self-terminating job after the last `.nam` exports), not on
  the laptop.
- **Cost shape.** RunPod 4090 "Secure Cloud" on-demand was ~$0.69/hr (not the
  ~$0.34 community rate assumed first). Budget against the secure rate.

**Outcome.** All 249 captures retrained to A2; merged best-of-2 set in
`training/a2-final/` (249 `.nam`). A2 is a net quality win — it lifts the worst
captures most (e.g. `AMP-2203-4` A1 0.050 → A2-Full ~0.019, ~2.6×) while easy
amps stay inaudibly good. Full per-model A1-vs-A2 numbers and bands:
`training/a2-out/ANALYSIS.md` and `training/a2-out/A1_vs_A2_diff.{csv,md}`.

## PowerShell + SSH gotchas (these wasted time)

The laptop drives pods over SSH/SCP from PowerShell (`podlib.ps1` wraps this).
The traps:

- **PowerShell expands `$` inside double-quoted strings**, even before sending
  to ssh. `\$` does NOT escape it. So `ssh ... "echo $?"` or `"$(seq ...)"` run
  on the *laptop* and send garbage. FIX: put all remote logic in a script,
  `scp` it, then run it. Do not inline `$`/`$(...)`/loops in the ssh string.
- Files written from Windows get CRLF; bash chokes on `\r`. After `scp`, run
  `sed -i 's/\r$//' <file>` before executing.
- pip/conda progress bars spam captured output; set
  `$ProgressPreference='SilentlyContinue'` for clean PowerShell output.
- Use the explicit HTTPS remote for git on this machine (see
  `.cursor/rules/git-remote-https.mdc`).

## Data: capture set + input sweep

- **Map by basename, not folder.** The 249 capture WAV basenames match the 249
  `rigs/**/*.nam` basenames EXACTLY (verified, globally unique). So
  `<name>.wav` → `<name>.nam`, and the trained model swaps back into whatever
  `rigs/` folder holds `<name>.nam` regardless of folder-name differences.
- **Input sweep is NAM's standard v3 input** (`T3K-sweep-v3.wav`), md5
  `36cd1af62985c2fac3e654333e36431e`, 48 kHz mono PCM_24, 190 s. It is **not
  stored in the repo** (large + re-downloadable from the official NAM input);
  upload it to the pod alongside the captures. Because it's a recognized
  standard input, the simplified trainer auto-handles latency + train/val
  splits. No re-capture needed for A2.

## Key learnings about A2 training

- Use the **simplified** trainer `nam.train.core.train(...)` — do NOT hand-roll
  data configs. Signature (`neural-amp-modeler` 0.13.0):
  `train(input_path, output_path, train_path, epochs=100, latency=None,
  batch_size=16, ny=8192, seed=0, save_plot=False, silent=False,
  modelname='model', ignore_checks=False, local=False, threshold_esr=None,
  user_metadata=None, fast_dev_run=False) -> TrainOutput(model, metadata)`.
- It auto-detects v3 (md5), auto-calibrates **per-file latency** from the
  calibration blips (varies a lot per capture, e.g. AMP-Ampt-1 = 4055 samples,
  AMP-2203-1 = -2 — this is expected and correct), runs v3 data checks, then
  trains. Use `ignore_checks=True` so a flagged check warns but still trains
  (these recordings already produced good A1 models).
- `_get_configs(v3, ...)` yields the **A2 recipe**: `PackedWaveNet`
  (SlimmableContainer) with submodels `channels_3` (A2-Lite) + `channels_8`
  (A2-Full), LeakyReLU, loss = ESR + MRSTFT (0.0005), lr 0.004 ExponentialLR
  gamma 0.994, trainer `{max_epochs, accelerator: gpu, devices: 1}`. Total
  params ~22.8K (tiny).
- **Best-fit is built in and per-submodel.** The trainer keeps the
  best-validation-ESR checkpoint across all epochs (`packed_best_submodel_*.ckpt`,
  `packed_best.json`) and loads it before export. So `epochs=700` is an UPPER
  BOUND: extra epochs never hurt quality (best is taken), they only cost time.
- `to.metadata.validation_esr` is the **aggregate** (sum of both submodels) and
  is dominated by Lite. For QA of the Full model, read the **channels_8** ESR
  (printed at export as "Error-signal ratio (channels_8)", or from
  `packed_best.json`).
- **Batch size kills quality (measured, AMP-2203-4, 700ep).** batch16 Full ESR
  0.0189 (27.7 min) vs batch64 0.0334 vs batch256 0.0698. Smaller batch = more
  updates = better fit. Do NOT raise batch.
- **Hard amps converge late.** AMP-2203-4 Full ESR was within 5% of best at
  ~epoch 468, 1% at ~591, then flat to 700. Easy amps plateau by ~150–200.
  Cutting epochs to 400 would hurt the hard amps; >700 is unnecessary.
  **700/best-fit is the sweet spot.**
- **A1 baseline (local, no Tone3000 needed).** Every `rigs/**/*.nam` embeds
  `metadata.training.validation_esr`. All 249 A1 models are "standard" WaveNet,
  16-ch: min 0.00013, median 0.0050, mean 0.0065, max 0.050 (worst = high-gain
  `AMP-2203-4`, `G12-Tri2-2`, Orange/H&K). The Tone3000 API exposes NO ESR/epoch
  data, so ignore it.

## DECISION: ship the packed SlimmableContainer (Lite + Full); VoLum plays Full

An earlier plan exported Full-only on the belief that VoLum's `ContainerModel`
defaults to the SMALLEST slice. That was WRONG. The vendored NAMCore defaults to
the LARGEST (Full) slice:

```cpp
// NeuralAmpModelerCore/NAM/container.cpp, ContainerModel ctor
_active_index = _submodels.size() - 1;   // Default to full size (last submodel)
```

Consequences, all verified:

- VoLum loads rigs via `nam::get_dsp(path)`, which dispatches on the
  `architecture` field through `ConfigParserRegistry`. `container.cpp`
  self-registers `"SlimmableContainer"`, so packed A2 files load with **no
  plugin change** and run the **Full** slice by default. (Core test
  `test_nam_rigs.cpp` "Core slimmable NAM example loads and processes" covers
  this path.)
- The standalone Full slice (plain `WaveNet`) is bit-identical DSP to the Full
  slice inside the packed container — so sound in VoLum is the same either way.
- A plain-`WaveNet` file has only one model inside, so the NAM Gateway shows **no
  Slim/Thin toggle**. The packed `SlimmableContainer` shows it. That cosmetic
  difference in Gateway is what surfaced this; it never indicated a training bug.

Chosen export (standard A2 format — what Tone3000 ships):

```python
to.model.net.export(Path(OUT), basename=name)   # PackedWaveNet.export()
```

`PackedWaveNet.export()` calls `export_container()` and writes architecture
`"SlimmableContainer"` with submodels `channels_3` (max_value 0.5, Lite) +
`channels_8` (max_value 1.0, Full), syncing loudness/gain metadata from the Full
slice. Verified against a Tone3000 A2 reference and by a 5-epoch smoke export.

Benefits: standard A2 format, Gateway toggle works, future-proof, and VoLum
still plays Full with zero plugin work.

Lite/CPU-saver mode in VoLum (a Full/Lite selector that calls
`SetSlimmableSize(0.0)`) is tracked in `backlog/F4-a2-lite-mode-support.md`.
Default stays Full.

## Run a round (operational)

The full operational sketch — provision pods, upload captures, `setup_pod.sh`,
`Fleet-Launch`, watch, collect, `Fleet-Teardown` — is in
[`training/cloud/README.md`](../training/cloud/README.md). Locked defaults:
`core.train`, A2, `batch_size=16`, `epochs=700`, best-fit, exported as packed
SlimmableContainer; escalation OFF; cap CPU threads and run many shards per GPU.

## Collect + QA

- Pull results with `Pod-ScpFrom`; merge `results_*.csv`; flag any `status != ok`
  or high channels_8 ESR (A2-Full should be roughly half the A1 ESR; Tone3000's
  median A2-Full ≈ 0.0033). For genuine under-fit outliers, prefer a **best-of-N**
  re-run over more epochs.
- Inspect any `.nam` with `training/cloud/check_nam.py` to confirm it's a packed
  `SlimmableContainer` (submodels channels_3 max 0.5 + channels_8 max 1.0).
- A1-vs-A2 diff + escalation report: `training/cloud/analyze.ps1`.

## Integrate into VoLum (separate session)

- Swap each `training/a2-final/<name>.nam` into the `rigs/**/<name>.nam` of the
  same basename (script the mapping; folders differ).
- Run rig load tests: `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
  (esp. `test_nam_rigs.cpp` + `test_volum_pre_pedal_captures.cpp`).
- Append a `NeuralAmpModeler/installer/changelog.txt` line.
- Packaging/release is a separate opt-in step.

## State

- **DONE (RunPod, 2026-06-06/07):** full 249-capture batch + round-2 best-of-2
  pass on ~30 candidates. Final merged set: `training/a2-final/` (249 packed
  `.nam` + `round_provenance.csv`). Round outputs: `training/a2-out/` (r1),
  `training/a2-r2-out/` (r2). Analysis: `training/a2-out/ANALYSIS.md`,
  `A1_vs_A2_diff.{csv,md}`. Reusable toolkit promoted to `training/cloud/`.
  All pods terminated; RunPod network volume deleted.
- **Remaining:** integrate `a2-final` into `rigs/` (separate session). The local
  `a2-*` output dirs are gitignored (large + redundant; final models land in
  `rigs/` via migration).
