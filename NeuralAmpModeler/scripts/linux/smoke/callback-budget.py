#!/usr/bin/env python3
"""Classify one standalone smoke window.

The integer rules match iplug::VoLumLinuxCallbackPeriodUs,
VoLumLinuxCallbackBudgetUs, VoLumLinuxCallbackP99Index and
VoLumLinuxClassifyCallbackWindow in VoLumAppLinuxPolicy.h. No callback may
exceed the period, and the 99th-percentile callback may use 700 thousandths
of it.
"""

import argparse
import re
import sys

# kVoLumLinuxCallbackBudgetThousandths
BUDGET_THOUSANDTHS = 700


def period_us(frames, rate):
    if frames <= 0 or rate <= 0:
        return 0
    return (frames * 1000000 + rate - 1) // rate


def budget_us(frames, rate):
    return period_us(frames, rate) * BUDGET_THOUSANDTHS // 1000


def p99_index(n):
    return 0 if n == 0 else min(n - 1, (n * 99) // 100)


def classify(max_us, p99_us, frames, rate, jack_xruns, strict, timing_complete):
    if not timing_complete or frames <= 0 or rate <= 0:
        return "fail-missing"
    if max_us > period_us(frames, rate):
        return "fail-callback"
    if p99_us > budget_us(frames, rate):
        return "fail-load"
    if jack_xruns > 0:
        return "fail-strict" if strict else "driver"
    return "pass"


def parse_rows(text):
    rows = []
    header = None
    for line in text.splitlines():
        if line.startswith("cb-timing "):
            header = line
        m = re.match(r"cb-timing-row end_ns=(\d+) dur_us=(\d+)\s*$", line)
        if m:
            rows.append((int(m.group(1)), int(m.group(2))))
    return header, rows


def header_field(header, key):
    if not header:
        return None
    m = re.search(r"\b" + key + r"=(\d+)", header)
    return int(m.group(1)) if m else None


def probe_field(text, key):
    vals = re.findall(r"\b" + key + r"=(\d+)", text)
    return int(vals[-1]) if vals else None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", required=True)
    parser.add_argument("--probe", required=True)
    parser.add_argument("--frames", type=int, required=True)
    parser.add_argument("--rate", type=int, required=True)
    parser.add_argument("--jack-xruns", type=int, required=True)
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()

    try:
        log = open(args.log, errors="replace").read()
        probe = open(args.probe, errors="replace").read()
    except OSError as exc:
        print("verdict=fail-missing")
        print("reason=%s" % exc)
        return 1

    header, rows = parse_rows(log)
    start = probe_field(probe, "window_start_ns")
    end = probe_field(probe, "window_end_ns")
    overflow = header_field(header, "overflow")
    logged_count = header_field(header, "count")
    logged_frames = header_field(header, "frames")
    logged_rate = header_field(header, "rate")
    period = period_us(args.frames, args.rate)
    budget = budget_us(args.frames, args.rate)

    # 20 ms past the probe's end stamp still belongs to the window, not to the quit.
    slack_ns = 20000000
    in_window = [
        dur
        for ns, dur in rows
        if start is not None and end is not None and start <= ns <= end + slack_ns
    ]
    complete = (
        header is not None
        and start is not None
        and end is not None
        and end >= start
        and overflow == 0
        and logged_count == len(rows)
        and logged_frames == args.frames
        and logged_rate == args.rate
        and len(in_window) > 0
    )
    in_window.sort()
    max_us = in_window[-1] if in_window else 0
    p50_us = in_window[len(in_window) // 2] if in_window else 0
    p99_us = in_window[p99_index(len(in_window))] if in_window else 0
    verdict = classify(max_us, p99_us, args.frames, args.rate, args.jack_xruns, args.strict, complete)
    print("window_p50_us=%d" % p50_us)
    print("window_p99_us=%d" % p99_us)
    print("window_max_us=%d" % max_us)
    print("window_callbacks=%d" % len(in_window))
    print("period_us=%d" % period)
    print("budget_us=%d" % budget)
    print("budget_thousandths=%d" % BUDGET_THOUSANDTHS)
    print("jack_xruns=%d" % args.jack_xruns)
    print("verdict=%s" % verdict)
    if verdict == "driver":
        print(
            "driver xruns: %d (not gated on WSL, root cause reports/17-jack-xrun.md)"
            % args.jack_xruns
        )
    if verdict == "fail-missing":
        print("reason=timing log does not cover the measured window")
    return 0 if verdict in ("pass", "driver") else 1


if __name__ == "__main__":
    sys.exit(main())
