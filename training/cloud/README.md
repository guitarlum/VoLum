# VoLum A2 cloud training toolkit

Scripts used to retrain VoLum's bundled captures from NAM A1 to A2 on rented
GPUs (RunPod), driven over SSH from this repo. This is the **operational**
reference (what each piece does, how to run a round). The **why** — design
decisions, measured numbers, and lessons — lives in
[`docs/a2-training-runbook.md`](../../docs/a2-training-runbook.md).

> Secrets: the RunPod API key is read from the gitignored repo-root `.env`
> (`RUNPOD_API_KEY=...`). Nothing here contains a key. `fleet.ps1` ships a
> worked example registry with **dead** pod IDs/IPs — replace before launching.

## Pieces

| File | Side | Purpose |
| --- | --- | --- |
| `podlib.ps1` | laptop | RunPod REST helpers (`Rp-Get/Post/Delete`) + SSH/SCP wrappers (`Pod-Run`, `Pod-Scp`, `Pod-ScpFrom`). Dot-source first. |
| `fleet.ps1` | laptop | Fleet registry (`$Fleet`, `$VolumeId`, `$NSHARD`) + ops: `Fleet-Launch`, `Fleet-Status`, `Fleet-Results`, `Fleet-OutCount`, `Fleet-Kill` (stop training), `Fleet-Teardown` (delete pods). Edit the registry per run. |
| `setup_pod.sh` | pod | One-shot dependency install (`tmux`, `python3-tk`, `neural-amp-modeler==0.13.0`, tensorboard) + import verification. Run once per fresh pod. |
| `launch_shards.sh` | pod | Starts `SHARD_LO..SHARD_HI` detached `tmux` training sessions, passing thread caps + escalation + output base. |
| `run_batch_cloud.sh` | pod | Per-shard wrapper: exports thread-cap env (`OMP/MKL/TORCH_THREADS=$THREADS`) then runs the trainer. |
| `batch_train_cloud.py` | pod | The trainer. Loops its shard of captures, trains A2 (`core.train`), exports the packed SlimmableContainer `.nam`, writes `results_<shard>.csv`, dumps per-epoch ESR `curves/<name>.json`. Honors `NAMES=` allow-list for targeted re-runs. Resumable (skips existing `.nam`). |
| `pod_status.sh` | pod | tmux sessions + log tails + results + GPU util. |
| `calib_run.sh` | pod | Single-shard calibration run for measuring s/epoch before committing the fleet. |
| `check_nam.py` | either | Inspect a `.nam`: architecture + submodels (channels/max_value). Confirms output is packed `SlimmableContainer`. |
| `analyze.ps1` | laptop | A1-vs-A2 ESR diff + escalation-candidate report from local `a2-out/` curves + `rigs/` baselines. |
| `monitor_loop.ps1` | laptop | Long-poll loop: out-count, ok/err/esc tallies, elapsed cost, cost guard. **Dies if the laptop sleeps** — see runbook. |
| `r2_watch.ps1` | laptop | Round-2 watcher with auto-download + local verify + **auto-teardown** of the pod on completion/timeout. Same sleep caveat. |

## Run a round (sketch)

```powershell
# 0. Key in repo-root .env: RUNPOD_API_KEY=...
. .\training\cloud\podlib.ps1 ; . .\training\cloud\fleet.ps1   # after editing $Fleet

# 1. Provision pods on a shared network volume (RunPod UI or API via podlib),
#    upload captures once to /workspace/captures_full, then per pod:
Pod-Scp $ip $port training\cloud\setup_pod.sh /workspace/a2/setup_pod.sh
Pod-Run $ip $port "bash /workspace/a2/setup_pod.sh"           # once per pod

# 2. Launch (epochs, escAt=100000 disables escalation):
Fleet-Launch 700 100000

# 3. Watch (don't let the laptop sleep), then collect + teardown:
.\training\cloud\monitor_loop.ps1
Fleet-Teardown                                                # DELETE pods when done
```

## Hard-won defaults (see runbook for the data)

- **Recipe is locked:** `core.train`, A2, `batch_size=16`, `epochs=700`,
  best-fit checkpoint, exported as packed SlimmableContainer. Don't raise batch
  (kills quality); don't cut epochs (hard amps converge late, ~600).
- **Validation-bound, not FLOPS-bound.** Cap CPU threads (`THREADS=2`, ~3 on a
  32-vCPU 4090) and run many shards per GPU; uncapped threads thrash the CPU.
- **Escalation off.** Auto-extending epochs when `best_epoch` was high chased
  6th-decimal noise. Run a flat 700; if you want more, do **best-of-N**.
- **Best-of-2 beats more epochs** for the genuinely under-fit minority:
  run the recipe twice and keep the lower-ESR `.nam` per capture.
- **Always tear pods down explicitly.** A local watcher cannot be trusted to do
  it if the laptop sleeps — prefer a pod-side self-terminating job for
  unattended runs.
