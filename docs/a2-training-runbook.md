# A2 Remote Retraining Runbook (handover)

Operational runbook for retraining VoLum's bundled captures from A1 to NAM
Architecture 2 (A2). Written as a handover so a future session (or a fresh
agent after context compression) can resume without re-discovering everything.

> **What we actually ran.** This doc was first written for a **WSL2-on-the-PC**
> plan (drive an RTX 4090 in the local PC over SSH). We pivoted and executed the
> real run on **rented RunPod GPUs** instead — it was faster, fully parallel,
> and avoided tying up the PC for days. The reusable cloud toolkit now lives in
> [`training/cloud/`](../training/cloud/) (`README.md` there is the operational
> guide). The WSL2 sections below are kept as a still-valid fallback; the
> **A2 recipe, packed-container decision, and basename mapping apply to both**.
> The cloud-specific lessons are in *"Learnings from the cloud run"* below.

## TL;DR of the approach

- GPU work cannot run over the network. Either rent GPUs (what we did — RunPod,
  many shards across a few 4090s on a shared network volume) **or** run in WSL2
  on a local GPU PC and drive it over SSH (the original fallback plan).
- The laptop's Cursor session only orchestrates: provisions/SSHes into the GPU
  hosts, launches sharded `tmux` training jobs, polls progress, and pulls
  results back. All PyTorch/CUDA + the NAM trainer run on the GPU host.

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
Next step (separate session): swap `training/a2-final/<name>.nam` into the
matching `rigs/**/<name>.nam`, bump to VoLum 1.1.0, ensure installers overwrite
old rig files.

## Machine roles

| Role | Host | Notes |
| --- | --- | --- |
| laptop | this machine (`c:\dev\VoLum`) | Cursor session, orchestrates over SSH. Has its own `Ubuntu-24.04` WSL used once to install the key. |
| PC | `192.168.178.91`, user `lum` | Windows 11 Home 25H2 + WSL2 `Ubuntu 26.04`, RTX 4090 (driver 595.x). |

SSH key (passwordless, key-only): `%USERPROFILE%\.ssh\volum_a2_pc`
(+ `.pub`). Connect with:

```powershell
ssh -i $env:USERPROFILE\.ssh\volum_a2_pc -o BatchMode=yes lum@192.168.178.91 "<cmd>"
```

## CRITICAL: PowerShell + SSH gotchas (these wasted time)

- **PowerShell expands `$` inside double-quoted strings**, even before sending
  to ssh. `\$` does NOT escape it. So `ssh ... "echo $?"` or `"$(seq ...)"` run
  on the *laptop* and send garbage. FIX: put all remote logic in a script,
  `scp` it, then run it. Do not inline `$`/`$(...)`/loops in the ssh string.
- Files written from Windows get CRLF; bash chokes on `\r`. After `scp`, run
  `sed -i 's/\r$//' <file>` before executing.
- `Test-NetConnection` and pip/conda progress bars spam the captured output;
  set `$ProgressPreference='SilentlyContinue'` for clean PowerShell output.
- Use the explicit HTTPS remote for git on this machine (see
  `.cursor/rules/git-remote-https.mdc`).

## Phase 0 — one-time PC setup (already done)

Done: WSL2 Ubuntu 26.04 installed; `networkingMode=mirrored` in
`%USERPROFILE%\.wslconfig`; `openssh-server` + systemd enabled; firewall TCP 22
+ Hyper-V inbound allowed; laptop public key installed in
`~/.ssh/authorized_keys`. Full step-by-step is in the plan file
`.cursor/plans/a2_remote_retraining_pipeline_*.plan.md` (Phase 0).

## Phase 1 — environment (already done)

On the PC WSL2, under `$HOME` (removable):

- Miniforge at `~/miniforge3`, conda env **`nam`** (Python 3.12).
- `torch 2.6.0+cu124` (CUDA works: `torch.cuda.is_available()` True, RTX 4090).
- `neural-amp-modeler 0.13.0`.

GPU libs note: `nvidia-smi` and `libcuda.so` live in `/usr/lib/wsl/lib`, which
is NOT on a non-login shell's PATH. Always export before running training:

```bash
source ~/miniforge3/etc/profile.d/conda.sh
conda activate nam
export LD_LIBRARY_PATH="/usr/lib/wsl/lib:${LD_LIBRARY_PATH:-}"
export MPLBACKEND=Agg
```

## Phase 2 — data (verified)

- Captures live on the PC at `C:\captures` → WSL `/mnt/c/captures`, mirroring
  `rigs/` folder names (folder names differ slightly, e.g.
  "Hughes & Kettner TriAmp Mk2" vs repo "H&K TriAmp Mk2" — IGNORE folders).
- **Map by basename, not folder.** The 249 capture WAV basenames match the 249
  `rigs/**/*.nam` basenames EXACTLY (verified, globally unique). So
  `<name>.wav` → `<name>.nam` and swaps back into whatever `rigs/` folder holds
  `<name>.nam`.
- Shared input sweep: `training/T3K-sweep-v3.wav` (also at
  `/mnt/c/captures/T3K-sweep-v3.wav`). **It is byte-identical to NAM's official
  input v3.0.0** (md5 `36cd1af62985c2fac3e654333e36431e`). 48 kHz, mono,
  PCM_24, 190 s. Because it's a recognized standard input, the simplified
  trainer auto-handles latency + train/val splits. No re-capture needed for A2.

## Key learnings about A2 training

- Use the **simplified** trainer `nam.train.core.train(...)` — do NOT hand-roll
  data configs. Signature (0.13.0):
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
  `packed_best.json`). Example: at 8 epochs, Full ESR ≈ 0.0071 already.

## DECISION: ship the packed SlimmableContainer (Lite + Full); VoLum plays Full

**Corrected 2026-06-05.** An earlier version of this runbook exported Full-only
on the belief that VoLum's `ContainerModel` defaults to the SMALLEST slice. That
was WRONG. The vendored NAMCore defaults to the LARGEST (Full) slice:

```cpp
// NeuralAmpModelerCore/NAM/container.cpp, ContainerModel ctor
_active_index = _submodels.size() - 1;   // Default to full size (last submodel)
```

Consequences, all verified:

- VoLum loads rigs via `nam::get_dsp(path)`, which dispatches on the
  `architecture` field through `ConfigParserRegistry`. `container.cpp`
  self-registers `"SlimmableContainer"`, so packed A2 files load with **no
  plugin change** and run the **Full** slice by default. (Core test
  `test_nam_rigs.cpp` "Core slimmable NAM example loads and processes" already
  covers this path.)
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
slice. Verified against the user's Tone3000 reference
`training/AMP-2203-32-A2.nam` and by a 5-epoch smoke export (same structure).

Benefits: standard A2 format, Gateway toggle works, future-proof, and VoLum
still plays Full with zero plugin work.

Lite/CPU-saver mode in VoLum (a Full/Lite selector that calls
`SetSlimmableSize(0.0)`) is a future feature: see
`backlog/F4-a2-lite-mode-support.md`. Default stays Full.

## Phase 3 — batch training

Scripts (canonical copies in `training/scripts/`, also deployed to `~/a2/` on
the PC):

- `batch_train.py` — loops captures, trains A2 at `EPOCHS` (default 700),
  exports the **packed SlimmableContainer** `.nam` to `~/a2/out/<name>.nam`
  (`PackedWaveNet.export()`; VoLum plays Full, Gateway shows the toggle), logs to
  `~/a2/results_<SHARD>.csv`, per-model logs in `~/a2/logs/`. Skips models whose
  `.nam` already exists (resumable). Shard via `SHARD`/`NSHARD` env so multiple
  workers run disjoint subsets on the one GPU (the model is tiny; 3-way parallel
  measured ~5.8 s/epoch/model with only ~5.4 GB VRAM used, so more shards may
  help — tune `NSHARD` and re-measure).
- `run_batch.sh` — sets env and runs `batch_train.py`.
- `pilot_one.py` — trains one capture full, keeps the run dir, dumps
  `summary.json` + `curves.json` (TensorBoard scalars) for ESR-vs-epoch
  analysis.
- `status.sh` — prints tmux sessions, log tails, results, GPU utilization.
- `run_remote_py.ps1` (laptop) — runs any local `.py` on the PC the safe way:
  scp + CRLF-strip + miniforge `nam` env prelude, so you never hand-quote ssh.
  E.g. `./training/scripts/run_remote_py.ps1 -Script training/scripts/train_named.py -EnvVars "NAMES=AMP-2203-4 EPOCHS=5"`.
- `nam_info.ps1` (laptop) — prints architecture + submodels (channels/max_value)
  + loudness for any `.nam`; use it to confirm output is packed `SlimmableContainer`.

Throughput (MEASURED, 3-way parallel pilots): ~5.8 s/epoch/model wall →
~67 min/model at 700 epochs, 3 running concurrently → ~22 min/model effective
throughput → **249 models ≈ 93 h ≈ ~3.9 days** of continuous GPU. VRAM at
3-way is only ~5.4 GB of 24 GB, so there is headroom to push to 5–6 shards;
GPU compute is the real limit (nvidia-smi ~98% at 3-way, but that metric
over-reads for tiny models — validate actual throughput before trusting it).
Launch pattern (tune `NSHARD`/loop count after measuring):

```bash
for k in 0 1 2 3 4; do
  tmux new-session -d -s train$k \
    "SHARD=$k NSHARD=5 EPOCHS=700 bash ~/a2/run_batch.sh > ~/a2/batch$k.log 2>&1"
done
```

### Pause / resume (e.g. to game on the PC)

Training fully saturates the GPU, so pause it before gaming.

- **Pause:** `tmux kill-server` (or `tmux kill-session -t train0 …`) on the PC.
  Any in-flight model is abandoned mid-epoch; its `.nam` was not written yet, so
  it simply re-trains from scratch on resume. All already-exported
  `~/a2/out/*.nam` are kept.
- **Resume:** re-run the launch loop above. `batch_train.py` skips every model
  whose `~/a2/out/<name>.nam` already exists, so it picks up where it left off.
- **Check progress any time:** `bash ~/a2/status.sh` and
  `ls ~/a2/out | wc -l` (count of finished models out of 249).
- Survives WSL/PC reboot the same way — just relaunch.

## Phase 4 — collect + QA

- Pull results: `scp -i <key> -r lum@192.168.178.91:~/a2/out c:\dev\VoLum\training\a2-out`.
- Merge `results_*.csv`; flag any `status != ok` or high channels_8 ESR
  (A2-Full should be roughly half the A1 ESR; Tone3000's median A2-Full ≈
  0.0033). Re-run outliers with more epochs if channels_8 best epoch was near
  the cap.

## Phase 5 — integrate into VoLum

- Swap each `out/<name>.nam` into the `rigs/**/<name>.nam` of the same basename
  (script the mapping; folders differ).
- Run rig load tests: `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
  (esp. `test_nam_rigs.cpp` + `test_volum_pre_pedal_captures.cpp`).
- Append a `NeuralAmpModeler/installer/changelog.txt` line.
- Packaging/release is a separate opt-in step.

## Current state (update me as we go)

Updated 2026-06-05 ~16:30.

- Phases 0–2 done. Phase 1 env verified. Mapping verified (249↔249).
- **EXPORT FORMAT CORRECTED to packed SlimmableContainer** (was Full-only). The
  Full-only premise (that VoLum defaults a packed file to Lite) was wrong;
  NAMCore `ContainerModel` defaults to the Full slice. `batch_train.py`,
  `train_named.py`, `pilot_one.py` now call `to.model.net.export(...)`. Verified
  by a 5-epoch smoke export: architecture `SlimmableContainer`, submodels
  channels_3 (max 0.5) + channels_8 (max 1.0), loudness synced — structurally
  identical to Tone3000's `training/AMP-2203-32-A2.nam`. See the DECISION section.
- **VoLum A2 compatibility CONFIRMED.** An A2-Full WaveNet was swapped into
  `rigs/Ampete One/AMP-Ampt-1.nam` and the Windows test exe passed 7/7 rig cases,
  62,305 assertions, incl. "Load all bundled" (240) + "Process one block". The
  packed-container load path is additionally covered by `test_nam_rigs.cpp`
  "Core slimmable NAM example loads and processes". Both load with **no plugin
  change**; packed defaults to Full.
- **Pilot ESR analysis (3 parallel 700-epoch runs).** Full-slice ESR
  (`ESR_packed_1`, ~62 steps/epoch):
  - AMP-Ampt-1: best Full ESR ≈ 0.00116, within 1% of best by ~epoch 142.
  - AMP-Herb-1: best Full ESR ≈ 0.00161, within 1% of best by ~epoch 180.
  - FX-PettyJohn-Nuke-1: best Full ESR ≈ 0.00145, within 1% of best by ~epoch 167.
  - Read curves with `training/scripts/parse_curve.py` (uses `ESR_packed_1`;
    `ROOT=~/a2/ab_runs` env to point at other run dirs). These 3 are EASY amps
    (lowest A1 ESR) and plateau by ~epoch 150–200 — NOT representative.
- **A1 baseline (local, no Tone3000 needed).** Every `rigs/**/*.nam` embeds
  `metadata.training.validation_esr`. All 249 are A1 "standard" WaveNet, 16-ch.
  A1 ESR across the library: min 0.00013, median 0.0050, mean 0.0065, max 0.050
  (worst = high-gain `AMP-2203-4`, `G12-Tri2-2`, Orange/H&K). Scan with
  `training/scripts/scan_a1_esr.ps1`. (Tone3000 API exposes NO ESR/epoch data,
  so it is useless for this; ignore it.)
- **A2-Full vs A1 — architecture + quality reality.** A2-Full is `channels_8`
  (8-ch, 23 layers deep, LeakyReLU + MRSTFT); A1 is 16-ch Tanh. On EASY amps A1
  has marginally lower ESR (e.g. AMP-Ampt-1 A1 0.00064 vs A2 0.00116) — both
  inaudibly good. On the HARDEST amp A2-Full WINS decisively:
  `AMP-2203-4` A1 0.050 → **A2-Full 0.0189 (~2.6× better)**. Net: A2 lifts the
  worst captures most; easy ones stay excellent. Migration is a quality win.
- **Epoch sizing CONFIRMED by hard-amp curve (AMP-2203-4, batch16, 700).** Full
  ESR within 5% of best at ~epoch 468, 2% at ~529, 1% at ~591, then flat to 700.
  Hard amps converge LATE (vs easy ~150), so cutting to 400 WOULD hurt them; but
  they flatten by ~600, so >700 is unnecessary. **700/best-fit is the sweet
  spot — keep it.**
- **Speed bake-off (AMP-2203-4, 700ep) — batch size kills quality.** batch16
  Full ESR 0.0189 (27.7 min) vs batch64 0.0334 (17.7 min) vs batch256 0.0698
  (20.4 min). Smaller batch = more updates = better fit. Do NOT raise batch.
  TF32 entangled but irrelevant. Parallelism only ~24% (validation-bound per
  epoch). No quality-preserving shortcut except validating every ~N epochs,
  which would require the full trainer — declined for a once-only run.
- **LOCKED RECIPE: `batch16, epochs=700, best-fit, A2` via `core.train`, exported
  as packed SlimmableContainer.** No batch increase, no epoch cut. ~4.8 days
  single / ~3.5 days at ~4–5 shards.
- **Epoch-escalation safety net added to `batch_train.py`** (user rule: amps that
  don't flatten by 700 get a short extension, not a full long retrain). If the
  Full best epoch ≥ `ESCALATE_AT` (660), it retrains at `ESCALATE_EPOCHS` (800)
  and keeps the better ESR;
  `results_<shard>.csv` now has `best_epoch`/`epochs_used`/`escalated` columns.
- **A/B build delivered.** The dev/installed standalone resolves rigs from the
  installer registry `VoLumRigsRoot = C:\Program Files\VoLum\VoLumRigs` (NOT repo
  `rigs/`). An AMP-2203-4 A2 model is placed there as
  `...\Marshall JMP 2203 1976\AMP-2203-A2.nam` (needs admin to write). In VoLum:
  amp Marshall JMP 2203 → speaker AMP → channel "4" (A1) vs "A2" (A2). NOTE: that
  A/B file was the old Full-only WaveNet; re-export it as packed (or just rely on
  the batch output) if you want the Gateway toggle on it too. TEMP artifact:
  delete from BOTH the installed rigs and any repo `rigs/` copy before the
  240-count rig test / commit.
- **Execution handover written:** `docs/a2-batch-handover-prompt.md` (paste into
  a new cheap-model session to run the full batch + collect + QA + integrate).

### DONE (RunPod cloud, 2026-06-06/07)

- Full 249-capture batch trained on RunPod (not WSL — see *"Learnings from the
  cloud run"* at the top), plus a round-2 best-of-2 pass on ~30 candidates.
- Final merged set: `training/a2-final/` (249 packed `.nam` + `round_provenance.csv`).
  Round outputs: `training/a2-out/` (r1) and `training/a2-r2-out/` (r2).
- Analysis: `training/a2-out/ANALYSIS.md`, `A1_vs_A2_diff.{csv,md}`.
- Reusable toolkit promoted to `training/cloud/` (see its `README.md`).
- Cloud cleanup done: all pods terminated; RunPod network volume deleted; A/B
  test file `rigs/.../AMP-2203-A2.nam` removed.
- **Remaining:** integrate `a2-final` into `rigs/` as VoLum 1.1.0 (separate
  session; handover prompt covers it).
