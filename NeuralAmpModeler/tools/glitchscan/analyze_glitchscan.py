"""Find dropouts, clicks and level jumps in a volum_glitchscan recording.

Channel L is the tone as rendered (the control), channel R is VoLum's output. The
tone's period is a whole number of samples, so each cycle is compared with the one
before it: steady audio leaves a small residual, a click or gap leaves a spike. That
"click" finding is only meaningful in steady stretches; around deliberate changes
read the "step" finding instead (see scan_channel).

R is VoLum's plugin output 2 only (both outputs are routed to device channel 2 and
the second overwrites the first), so with Dual Amp on it carries the Support lane.

  python analyze_glitchscan.py rec.wav [--events events.json] [--json out.json]

events.json is a list of {"t": unix_ms, "what": "..."}; each finding is tagged with
the nearest preceding event so a glitch can be attributed to the action that caused it.
"""

import argparse
import json
import struct
import sys

import numpy as np


def read_wav(path):
    with open(path, "rb") as f:
        head = f.read(4096)
    if head[:4] != b"RIFF" or head[8:12] != b"WAVE":
        raise ValueError("not a WAV file")
    pos = 12
    rate = ch = None
    while pos + 8 <= len(head):
        cid = head[pos:pos + 4]
        size = struct.unpack("<I", head[pos + 4:pos + 8])[0]
        if cid == b"fmt ":
            _, ch, rate = struct.unpack("<HHI", head[pos + 8:pos + 16])
        elif cid == b"data":
            samples = np.memmap(path, dtype="<f4", mode="r", offset=pos + 8, shape=(size // 4,))
            return rate, samples.reshape(-1, ch)
        pos += 8 + size + (size & 1)
    raise ValueError("no data chunk")


def db(x):
    return 20.0 * np.log10(np.maximum(x, 1e-9))


def window_rms(x, n):
    k = len(x) // n
    return np.sqrt(np.mean(x[: k * n].reshape(k, n) ** 2, axis=1))


def runs(mask):
    """(start, length) of each run of True."""
    if not mask.any():
        return []
    d = np.diff(np.concatenate(([0], mask.astype(np.int8), [0])))
    starts = np.flatnonzero(d == 1)
    ends = np.flatnonzero(d == -1)
    return list(zip(starts.tolist(), (ends - starts).tolist()))


def scan_channel(x, rate, period, win_ms=10.0):
    n = int(rate * win_ms / 1000)
    rms = window_rms(x, n)
    rms_db = db(rms)
    findings = []

    # Exact-zero runs while the channel is otherwise live: a buffer that never came.
    live = rms_db > -60
    zero = x == 0.0
    for s, length in runs(zero):
        if length >= 32:
            w = min(s // n, len(live) - 1)
            neighbours = live[max(0, w - 20): w + 20]
            if neighbours.mean() > 0.5:
                findings.append({"kind": "zero-run", "frame": s, "ms": 1000.0 * length / rate})

    # Dropouts: a 10 ms window more than 12 dB under the median of the surrounding second.
    half = int(500 / win_ms)
    for i in range(len(rms_db)):
        lo, hi = max(0, i - half), min(len(rms_db), i + half)
        base = np.median(rms_db[lo:hi])
        if base > -50 and rms_db[i] < base - 12:
            findings.append({"kind": "dip", "frame": i * n, "depthDb": float(base - rms_db[i])})

    # Clicks: cycle-to-cycle residual spikes far above the local residual level.
    r = np.abs(x[period:] - x[:-period])
    rn = int(rate * 0.005)
    k = len(r) // rn
    peak = r[: k * rn].reshape(k, rn).max(axis=1)
    level = np.sqrt(np.mean(x[: k * rn].reshape(k, rn) ** 2, axis=1))
    for i in range(k):
        lo, hi = max(0, i - 100), min(k, i + 100)
        base = np.median(peak[lo:hi])
        ref = max(base, 0.02 * np.median(level[lo:hi]), 1e-4)
        if level[i] > 1e-3 and peak[i] > 8 * ref and peak[i] > 0.05 * np.max(level[lo:hi]) * 1.414:
            findings.append({"kind": "click", "frame": (i * rn) + period, "ratio": float(peak[i] / ref)})

    # Isolated hard steps: a 10 ms window whose largest sample-to-sample step is more
    # than twice the largest step anywhere else within +-1 s. A sound change moves
    # between two envelopes and does not trip this; a click or a dropped block does.
    # This is the finding to trust around deliberate changes (switching, toggles),
    # where the cycle residual above fires on every timbre change.
    st = np.abs(np.diff(x))
    k = len(st) // n
    m = st[: k * n].reshape(k, n).max(axis=1)
    for i in range(100, k - 100):
        ref = max(m[i - 100:i - 2].max(), m[i + 3:i + 100].max())
        if m[i] > 0.02 and m[i] > 2 * ref:
            findings.append({"kind": "step", "frame": i * n, "ratio": float(m[i] / ref)})

    return rms_db, merge(findings, rate)


def merge(findings, rate, gap_ms=50):
    """Collapse findings of the same kind closer than gap_ms into one."""
    out = []
    for f in sorted(findings, key=lambda f: (f["kind"], f["frame"])):
        if out and out[-1]["kind"] == f["kind"] and f["frame"] - out[-1]["lastFrame"] < rate * gap_ms / 1000:
            out[-1]["lastFrame"] = f["frame"]
            out[-1]["count"] += 1
            continue
        g = dict(f)
        g["lastFrame"] = f["frame"]
        g["count"] = 1
        out.append(g)
    return sorted(out, key=lambda f: f["frame"])


def level_jumps(rms_db, win_ms, rate, threshold_db=3.0):
    """100 ms median level steps larger than threshold_db."""
    step = int(100 / win_ms)
    k = len(rms_db) // step
    med = np.median(rms_db[: k * step].reshape(k, step), axis=1)
    out = []
    for i in range(1, k):
        if med[i - 1] > -50 or med[i] > -50:
            d = med[i] - med[i - 1]
            if abs(d) >= threshold_db:
                out.append({"kind": "level-step", "frame": int(i * step * int(rate * win_ms / 1000)), "deltaDb": float(d),
                            "fromDb": float(med[i - 1]), "toDb": float(med[i])})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wav")
    ap.add_argument("--events")
    ap.add_argument("--json")
    ap.add_argument("--skip", type=float, default=1.0, help="seconds to ignore at the start (stream warm-up)")
    ap.add_argument("--chunk", type=float, default=60.0, help="seconds analysed at a time")
    a = ap.parse_args()

    meta = json.load(open(a.wav + ".json"))
    rate, data = read_wav(a.wav)
    period = int(round(rate / meta["freq"]))
    skip = int(a.skip * rate)
    t0 = meta["firstFrameUnixMs"] or meta["startUnixMs"]
    events = json.load(open(a.events)) if a.events else []

    report = {"wav": a.wav, "seconds": len(data) / rate, "rate": rate, "wasapiDiscontinuities":
              meta["discontinuityFrames"], "channels": {}}
    chunk = int(a.chunk * rate)
    for name, idx in (("tone_L", 0), ("volum_R", 1)):
        # Chunked so a long recording fits in memory; a finding straddling a chunk
        # boundary can be missed, which is why the boundaries overlap by one second.
        rms_parts, finds, jumps = [], [], []
        for start in range(skip, len(data), chunk):
            x = data[max(skip, start - rate):start + chunk, idx].astype(np.float32)
            base = max(skip, start - rate)
            part_db, part_finds = scan_channel(x, rate, period)
            part_jumps = level_jumps(part_db, 10.0, rate)
            drop = start - base
            for f in part_finds + part_jumps:
                if f["frame"] >= drop or start == skip:
                    f["frame"] += base
                    (jumps if f["kind"] == "level-step" else finds).append(f)
            rms_parts.append(part_db[int(drop / (rate * 0.01)):])
        rms_db = np.concatenate(rms_parts) if rms_parts else np.array([])
        x = data[skip:, idx]
        for f in finds + jumps:
            f["t"] = f["frame"] / rate
            ms = t0 + 1000.0 * f["t"]
            prior = [e for e in events if e["t"] <= ms + 250]
            f["after"] = (prior[-1]["what"] + " (+%.0f ms)" % (ms - prior[-1]["t"])) if prior else None
            f.pop("lastFrame", None)
        live = rms_db[rms_db > -60]
        report["channels"][name] = {
            "medianDb": float(np.median(rms_db)) if len(rms_db) else None,
            "liveFraction": float(len(live) / max(1, len(rms_db))),
            "peak": float(np.max(np.abs(x))) if len(x) else 0.0,
            "findings": finds,
            "levelSteps": jumps,
        }

    for name, c in report["channels"].items():
        kinds = {}
        for f in c["findings"]:
            kinds[f["kind"]] = kinds.get(f["kind"], 0) + 1
        print("%-8s median %6.1f dB  live %.2f  peak %.3f  %s  level-steps %d" % (
            name, c["medianDb"], c["liveFraction"], c["peak"], kinds or "clean", len(c["levelSteps"])))
        for f in c["findings"][:40]:
            print("   %8.3fs %-9s %s  after: %s" % (f["t"], f["kind"], {k: v for k, v in f.items() if k in (
                "ms", "depthDb", "ratio", "count")}, f["after"]))
    print("wasapi discontinuities: %d" % len(meta["discontinuityFrames"]))
    if a.json:
        json.dump(report, open(a.json, "w"), indent=1)


if __name__ == "__main__":
    sys.exit(main())
