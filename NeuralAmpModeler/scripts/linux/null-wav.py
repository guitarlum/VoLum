#!/usr/bin/env python3
"""Peak residual of two WAV renders, in dBFS where 0 dBFS is amplitude 1.

Exit 0 when the residual is at or below --limit-db (default -100, the same
bar as the Linux-vs-Windows null in platform_null.h) and the live file's own
peak is above --floor-db (default -40), so a silent render cannot pass.
"""

import math
import struct
import sys


def _read_chunk(data, offset):
    if offset + 8 > len(data):
        raise ValueError("truncated chunk")
    tag = data[offset : offset + 4]
    size = struct.unpack_from("<I", data, offset + 4)[0]
    start = offset + 8
    end = start + size
    if end > len(data):
        raise ValueError(f"truncated {tag!r}")
    return tag, data[start:end], end + (size & 1)


def read_wav(path):
    blob = open(path, "rb").read()
    if blob[0:4] != b"RIFF" or blob[8:12] != b"WAVE":
        raise ValueError(f"{path} is not a WAV")
    fmt = None
    pcm = None
    offset = 12
    while offset + 8 <= len(blob):
        tag, body, offset = _read_chunk(blob, offset)
        if tag == b"fmt ":
            fmt = body
        elif tag == b"data":
            pcm = body
            break
    if fmt is None or pcm is None:
        raise ValueError(f"{path} is missing fmt or data")
    if len(fmt) < 16:
        raise ValueError(f"{path} fmt chunk is short")
    audio_format, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", fmt)
    if audio_format == 0xFFFE:
        if len(fmt) < 26:
            raise ValueError(f"{path} extensible fmt is short")
        audio_format = struct.unpack_from("<H", fmt, 24)[0]
    width = bits // 8
    if channels < 1 or width < 1 or len(pcm) % (channels * width) != 0:
        raise ValueError(f"{path} has a broken frame size")
    frames = len(pcm) // (channels * width)
    out = []
    for i in range(frames * channels):
        off = i * width
        if audio_format == 3 and bits == 32:
            out.append(struct.unpack_from("<f", pcm, off)[0])
        elif audio_format == 3 and bits == 64:
            out.append(struct.unpack_from("<d", pcm, off)[0])
        elif audio_format == 1 and bits == 16:
            out.append(struct.unpack_from("<h", pcm, off)[0] / 32768.0)
        elif audio_format == 1 and bits == 24:
            b = pcm[off : off + 3]
            v = b[0] | (b[1] << 8) | (b[2] << 16)
            if v & 0x800000:
                v -= 0x1000000
            out.append(v / 8388608.0)
        elif audio_format == 1 and bits == 32:
            out.append(struct.unpack_from("<i", pcm, off)[0] / 2147483648.0)
        else:
            raise ValueError(f"{path} format {audio_format} bits {bits} is not supported")
    return channels, rate, frames, out


def peak_db(values):
    peak = 0.0
    for v in values:
        if math.isnan(v) or math.isinf(v):
            return math.inf
        a = abs(v)
        if a > peak:
            peak = a
    if peak <= 0.0:
        return -math.inf
    return 20.0 * math.log10(peak)


def main() -> int:
    args = sys.argv[1:]
    limit = -100.0
    floor = -40.0
    paths = []
    i = 0
    while i < len(args):
        if args[i] == "--limit-db":
            limit = float(args[i + 1])
            i += 2
        elif args[i] == "--floor-db":
            floor = float(args[i + 1])
            i += 2
        else:
            paths.append(args[i])
            i += 1
    if len(paths) != 2:
        print("usage: null-wav.py [--limit-db -100] [--floor-db -40] LIVE.wav REF.wav", file=sys.stderr)
        return 2
    live_ch, live_rate, live_frames, live = read_wav(paths[0])
    ref_ch, ref_rate, ref_frames, ref = read_wav(paths[1])
    if live_rate != ref_rate:
        print(f"null FAIL sample rate {live_rate} vs {ref_rate}")
        return 1
    channels = min(live_ch, ref_ch)
    frames = min(live_frames, ref_frames)
    if frames < live_rate:  # under one second is not the 10 s render
        print(f"null FAIL only {frames} frames")
        return 1
    residual = 0.0
    live_peak = 0.0
    for n in range(frames):
        for c in range(channels):
            a = live[n * live_ch + c]
            b = ref[n * ref_ch + c]
            if math.isnan(a) or math.isnan(b) or math.isinf(a) or math.isinf(b):
                print("null FAIL non-finite sample")
                return 1
            d = abs(a - b)
            if d > residual:
                residual = d
            if abs(a) > live_peak:
                live_peak = abs(a)
    residual_db = -math.inf if residual <= 0.0 else 20.0 * math.log10(residual)
    live_db = -math.inf if live_peak <= 0.0 else 20.0 * math.log10(live_peak)
    print(
        f"null peak {residual_db:.2f} dBFS over {frames} frames x {channels} ch "
        f"(live peak {live_db:.2f} dBFS, limit {limit:.0f}, floor {floor:.0f})"
    )
    if live_db <= floor:
        print(f"null FAIL live peak {live_db:.2f} dBFS is at or below {floor:.0f}")
        return 1
    if residual_db > limit:
        print(f"null FAIL residual {residual_db:.2f} dBFS is above {limit:.0f}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
