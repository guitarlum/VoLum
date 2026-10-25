#!/usr/bin/env python3
"""Write a deterministic 10 s mono float32 DI for the REAPER render gate."""

import math
import struct
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: write-di.py OUT.wav", file=sys.stderr)
        return 2
    rate = 48000
    count = rate * 10
    onsets = (0.20, 2.40, 4.80, 7.20)
    samples = []
    for i in range(count):
        t = i / rate
        v = 0.0
        for onset in onsets:
            dt = t - onset
            if dt < 0.0:
                continue
            env = math.exp(-dt * 2.8)
            v += 0.22 * env * math.sin(2.0 * math.pi * 110.0 * dt)
            v += 0.07 * env * math.sin(2.0 * math.pi * 277.0 * dt)
            v += 0.04 * env * math.sin(2.0 * math.pi * 440.0 * dt * (1.0 + 0.002 * dt))
        if v > 1.0:
            v = 1.0
        elif v < -1.0:
            v = -1.0
        samples.append(v)
    data = struct.pack("<" + "f" * count, *samples)
    header = struct.pack(
        "<4sI4s4sIHHIIHH4sI",
        b"RIFF",
        36 + len(data),
        b"WAVE",
        b"fmt ",
        16,
        3,  # IEEE float
        1,
        rate,
        rate * 4,
        4,
        32,
        b"data",
        len(data),
    )
    with open(sys.argv[1], "wb") as out:
        out.write(header)
        out.write(data)
    print(f"di {sys.argv[1]} frames={count} rate={rate} peak={max(abs(s) for s in samples):.4f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
