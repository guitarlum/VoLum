"""
Cloud variant of training/scripts/batch_train.py for RunPod.

Identical LOCKED RECIPE (A2, batch_size=16, epochs=700, best-fit, packed
SlimmableContainer export, escalation safety net). ONLY difference vs the
canonical script: capture/base dirs are env-configurable for the pod layout
(network volume at /workspace), and there is no WSL/conda prelude.

Env:
  CAP_DIR   (default /workspace/captures)  dir with capture WAVs + T3K-sweep-v3.wav
  A2_BASE   (default /workspace/a2)        out/, runs/, logs/, results_<SHARD>.csv
  EPOCHS    (default 700)   upper bound; best-fit checkpoint is exported
  LIMIT     (default 0=all) cap captures (after sharding)
  SHARD     (default 0)     this worker's shard index
  NSHARD    (default 1)     total workers; handles captures where idx%NSHARD==SHARD
  BATCH     (default 16)    LOCKED; override only for experiments
  ESCALATE_AT     (default 660)
  ESCALATE_EPOCHS (default 800)
"""
import os, glob, time, csv, json, shutil, traceback, contextlib
from pathlib import Path

os.environ.setdefault("MPLBACKEND", "Agg")
import nam.train.core as core
import torch as _torch
from tensorboard.backend.event_processing import event_accumulator

# Cap intra-op threads so many shards can share the pod's vCPUs without thrashing
# (this workload is validation/CPU-bound; unconstrained threads oversubscribe).
_tt = os.environ.get("TORCH_THREADS")
if _tt:
    try:
        _torch.set_num_threads(int(_tt))
    except Exception:
        pass

CAP = os.environ.get("CAP_DIR", "/workspace/captures")
SWEEP = os.path.join(CAP, "T3K-sweep-v3.wav")
BASE = os.environ.get("A2_BASE", "/workspace/a2")
OUT = os.path.join(BASE, "out")
RUNS = os.path.join(BASE, "runs")
LOGS = os.path.join(BASE, "logs")
CURVES = os.path.join(BASE, "curves")
EPOCHS = int(os.environ.get("EPOCHS", "700"))
LIMIT = int(os.environ.get("LIMIT", "0"))
SHARD = int(os.environ.get("SHARD", "0"))
NSHARD = int(os.environ.get("NSHARD", "1"))
BATCH = int(os.environ.get("BATCH", "16"))
ESCALATE_AT = int(os.environ.get("ESCALATE_AT", "660"))
ESCALATE_EPOCHS = int(os.environ.get("ESCALATE_EPOCHS", "800"))
RESULTS = os.path.join(BASE, f"results_{SHARD}.csv")

for d in (OUT, RUNS, LOGS, CURVES):
    os.makedirs(d, exist_ok=True)
if not os.path.exists(SWEEP):
    raise SystemExit(f"Sweep not found: {SWEEP}")

caps = sorted(p for p in glob.glob(os.path.join(CAP, "**", "*.wav"), recursive=True)
              if os.path.basename(p) != "T3K-sweep-v3.wav")
# Optional explicit allow-list (comma-separated basenames) for targeted re-runs.
NAMES = os.environ.get("NAMES", "").strip()
if NAMES:
    wanted = set(n.strip() for n in NAMES.split(",") if n.strip())
    caps = [p for p in caps if Path(p).stem in wanted]
caps = [p for i, p in enumerate(caps) if i % NSHARD == SHARD]
if LIMIT > 0:
    caps = caps[:LIMIT]

new = not os.path.exists(RESULTS)
rf = open(RESULTS, "a", newline="")
w = csv.writer(rf)
if new:
    w.writerow(["name", "status", "full_esr", "best_epoch", "epochs_used", "escalated", "seconds", "output"])
    rf.flush()

total = len(caps)
print(f"[shard {SHARD}/{NSHARD}] {total} captures, epochs={EPOCHS}, batch={BATCH}, escalate_at={ESCALATE_AT}", flush=True)


def full_metrics(rundir, epochs):
    ev = glob.glob(os.path.join(rundir, "lightning_logs", "version_*", "events.out.tfevents.*"))
    if not ev:
        return None, None
    try:
        ea = event_accumulator.EventAccumulator(ev[0], size_guidance={event_accumulator.SCALARS: 0})
        ea.Reload()
        tags = ea.Tags().get("scalars", [])
        tag = "ESR_packed_1" if "ESR_packed_1" in tags else ("ESR" if "ESR" in tags else None)
        if tag is None:
            return None, None
        series = [(s.step, s.value) for s in ea.Scalars(tag)]
        if not series:
            return None, None
        last_step = series[-1][0] or 1
        best_step, best = min(series, key=lambda x: x[1])
        best_epoch = int(round(best_step / last_step * epochs))
        return best, best_epoch
    except Exception:
        return None, None


def dump_curve(rundir, name, epochs):
    """Persist the full per-epoch scalar curves (ESR_packed_0/1, losses, etc.)
    so a later pass can decide whether escalating this profile beyond 700 epochs
    is worthwhile. Written before the run dir is deleted. Best-effort: never raises.
    """
    try:
        ev = glob.glob(os.path.join(rundir, "lightning_logs", "version_*", "events.out.tfevents.*"))
        if not ev:
            return
        ea = event_accumulator.EventAccumulator(ev[0], size_guidance={event_accumulator.SCALARS: 0})
        ea.Reload()
        tags = ea.Tags().get("scalars", [])
        series = {t: [[s.step, float(s.value)] for s in ea.Scalars(t)] for t in tags}
        rec = {"name": name, "epochs": epochs, "tags": series}
        full = series.get("ESR_packed_1") or series.get("ESR")
        if full:
            last_step = full[-1][0] or 1
            best_step, best = min(full, key=lambda x: x[1])
            # ESR at ~90% of training, to quantify late-stage improvement vs best
            cut = 0.9 * last_step
            tail = [v for st, v in full if st >= cut] or [full[-1][1]]
            esr_at_90 = min(tail)
            rec["summary"] = {
                "best_esr": best,
                "best_epoch": int(round(best_step / last_step * epochs)),
                "final_esr": full[-1][1],
                "esr_at_90pct": esr_at_90,
                "late_gain_after_90pct": esr_at_90 - best,
            }
        with open(os.path.join(CURVES, name + ".json"), "w") as cf:
            json.dump(rec, cf)
    except Exception:
        pass


def train_once(out_wav, name, train_path, epochs, logf):
    with open(logf, "a") as lf, contextlib.redirect_stdout(lf), contextlib.redirect_stderr(lf):
        return core.train(SWEEP, out_wav, train_path, epochs=epochs, batch_size=BATCH,
                          silent=True, save_plot=False, modelname=name, ignore_checks=True)


done = 0
for i, out_wav in enumerate(caps, 1):
    name = Path(out_wav).stem
    nam_path = os.path.join(OUT, name + ".nam")
    if os.path.exists(nam_path):
        print(f"[{SHARD}][{i}/{total}] SKIP {name}", flush=True)
        done += 1
        continue
    logf = os.path.join(LOGS, name + ".log")
    open(logf, "w").close()
    train_path = os.path.join(RUNS, name)
    shutil.rmtree(train_path, ignore_errors=True)
    os.makedirs(train_path, exist_ok=True)
    t0 = time.time()
    print(f"[{SHARD}][{i}/{total}] TRAIN {name} (epochs={EPOCHS}) ...", flush=True)
    status, full_esr, best_epoch, epochs_used, escalated = "ok", None, None, EPOCHS, False
    try:
        to = train_once(out_wav, name, train_path, EPOCHS, logf)
        if to is None or to.model is None:
            status = "no_model"
        else:
            full_esr, best_epoch = full_metrics(train_path, EPOCHS)
            chosen_to = to
            if best_epoch is not None and best_epoch >= ESCALATE_AT and EPOCHS < ESCALATE_EPOCHS:
                print(f"[{SHARD}][{i}/{total}] ESCALATE {name}: best_epoch={best_epoch} >= {ESCALATE_AT}; "
                      f"retrain @ {ESCALATE_EPOCHS}", flush=True)
                esc_path = os.path.join(RUNS, name + "_esc")
                shutil.rmtree(esc_path, ignore_errors=True)
                os.makedirs(esc_path, exist_ok=True)
                to2 = train_once(out_wav, name, esc_path, ESCALATE_EPOCHS, logf)
                esr2, ep2 = full_metrics(esc_path, ESCALATE_EPOCHS)
                if to2 is None or to2.model is None or esr2 is None:
                    raise RuntimeError(f"Escalation retrain did not finish cleanly for {name}")
                escalated = True
                if esr2 is not None and (full_esr is None or esr2 < full_esr):
                    chosen_to, full_esr, best_epoch, epochs_used, escalated = to2, esr2, ep2, ESCALATE_EPOCHS, True
                shutil.rmtree(esc_path, ignore_errors=True)
            chosen_to.model.net.export(Path(OUT), basename=name)
            if not os.path.exists(nam_path):
                status = "export_missing"
    except Exception as e:
        status = "error:" + type(e).__name__
        with open(logf, "a") as lf:
            lf.write("\n" + traceback.format_exc())
    dt = time.time() - t0
    if status == "ok":
        dump_curve(train_path, name, EPOCHS)
        shutil.rmtree(train_path, ignore_errors=True)
    w.writerow([name, status, full_esr, best_epoch, epochs_used, escalated, round(dt, 1),
                nam_path if status == "ok" else ""])
    rf.flush()
    done += 1
    print(f"[{SHARD}][{i}/{total}] {status} {name} full_esr={full_esr} best_epoch={best_epoch} "
          f"escalated={escalated} {dt:.1f}s", flush=True)

print(f"[shard {SHARD}/{NSHARD}] complete: {done}/{total}", flush=True)
rf.close()
