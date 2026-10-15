#!/usr/bin/env python3
"""Helpers for e2e-standalone-mac.sh: library JSON, a virtual CoreMIDI source,
window geometry, synthetic clicks and keys, and the screenshot blank check.

  python3 e2e_standalone_mac.py <command> [args...]

Needs python-rtmidi (midi-serve), pyobjc-framework-Quartz (window, click,
click-canvas, key, screen-access) and Pillow (variance, flat-png, noise-png).
"""

import json
import os
import random
import struct
import sys
import time
import wave

CANVAS_W = 900.0
CANVAS_H = 600.0
KINDS = (("amps", "customAmps"), ("irs", "irLibrary"), ("pedals", "customPedals"), ("scenes", "customScenes"))


def load(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def save(path, data):
    tmp = path + ".e2e-tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    os.replace(tmp, path)


def text_of(value):
    if isinstance(value, bool):
        return "true" if value else "false"
    if value is None:
        return "null"
    if isinstance(value, str):
        return value
    if isinstance(value, (int, float)):
        return str(value)
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def map_key(reg):
    rows = reg.get("midiSoundMap") or []
    rows = sorted(rows, key=lambda r: int(r.get("slot", 0)))
    return "|".join("{}:{}:{}".format(r.get("slot"), r.get("ampId"), r.get("presetId")) for r in rows)


def content_ids(reg):
    ids = {}
    for kind, key in KINDS:
        ids[kind] = sorted(str(item.get("id")) for item in (reg.get(key) or []) if isinstance(item, dict))
    banks = reg.get("presetBanks") or {}
    ids["presetBanks"] = sorted(banks.keys()) if isinstance(banks, dict) else []
    return ids


def write_ir(path):
    # A decaying noise burst: a cab-like IR any .wav reader accepts (mono, 16-bit, 48 kHz).
    os.makedirs(os.path.dirname(path), exist_ok=True)
    rng = random.Random(1300)
    frames = bytearray()
    for i in range(4096):
        amp = 0.6 * (0.9985 ** i)
        frames += struct.pack("<h", int(max(-1.0, min(1.0, rng.uniform(-1.0, 1.0) * amp)) * 32767))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(48000)
        w.writeframes(bytes(frames))


def cmd_json_get(path, key):
    try:
        data = load(path)
    except FileNotFoundError:
        print("<missing file>")
        return 0
    except ValueError:
        print("<unreadable>")
        return 0
    print("<missing>" if key not in data else text_of(data[key]))
    return 0


def cmd_json_set(path, key, raw):
    data = load(path)
    data[key] = json.loads(raw)
    save(path, data)
    return 0


def cmd_parses(path):
    try:
        load(path)
        return 0
    except (OSError, ValueError):
        return 1


def cmd_has_key(path, key):
    try:
        return 0 if key in load(path) else 1
    except (OSError, ValueError):
        return 1


def cmd_mapkey(path):
    try:
        print(map_key(load(path)))
    except (OSError, ValueError):
        print("<no registry>")
    return 0


def cmd_clear_map(path):
    data = load(path)
    data["midiSoundMap"] = []
    save(path, data)
    return 0


def cmd_move_slot(path, slot, preset_id):
    data = load(path)
    for row in data.get("midiSoundMap") or []:
        if int(row.get("slot", -1)) == int(slot):
            row["presetId"] = preset_id
            save(path, data)
            print("moved")
            return 0
    print("no row for slot " + slot)
    return 1


def cmd_rewind_v2(path, ir_id, ir_rel):
    """What 1.2.0 would have written: schema v2, no midiSoundMap, no IR shaping keys.
    One IR this run generates is added so the trim migration has something to measure."""
    data = load(path)
    content_dir = os.path.dirname(path)
    write_ir(os.path.join(content_dir, ir_rel))
    irs = [ir for ir in (data.get("irLibrary") or []) if ir.get("id") != ir_id]
    irs.append({"id": ir_id, "name": "E2E Cab", "path": ir_rel})
    for ir in irs:
        for field in ("trimDb", "lowCutHz", "highCutHz"):
            ir.pop(field, None)
    data["irLibrary"] = irs
    data["schemaVersion"] = 2
    data.pop("midiSoundMap", None)
    save(path, data)
    return 0


def cmd_measurable(path):
    data = load(path)
    content_dir = os.path.dirname(path)
    print(sum(1 for ir in (data.get("irLibrary") or []) if ir.get("path") and os.path.isfile(os.path.join(content_dir, ir["path"]))))
    return 0


def cmd_calibrated(path):
    data = load(path)
    print(sum(1 for ir in (data.get("irLibrary") or []) if ir.get("trimDb") is not None))
    return 0


def cmd_count(path, key):
    data = load(path)
    print(len(data.get(key) or []))
    return 0


def cmd_ir_json(path):
    try:
        print(json.dumps(load(path).get("irLibrary"), sort_keys=True, separators=(",", ":")))
    except (OSError, ValueError):
        print("<no registry>")
    return 0


def cmd_lost(before_path, after_path):
    before = content_ids(load(before_path))
    try:
        after = content_ids(load(after_path))
    except (OSError, ValueError):
        after = {k: [] for k in before}
    for kind in ("amps", "irs", "pedals", "scenes", "presetBanks"):
        missing = [i for i in before[kind] if i not in after[kind]]
        print("{}={}".format(kind, ",".join(missing)))
    return 0


def cmd_seed_board(path, schema):
    # Known Factory Sounds on scattered programs; 7 is left empty on purpose.
    board = {3: ("factory:12", "factory:12:v1"), 9: ("factory:6", "factory:6:v1"), 42: ("factory:0", "factory:0:v2"),
             100: ("factory:8", "factory:8:v1"), 101: ("factory:13", "factory:13:v2")}
    os.makedirs(os.path.dirname(path), exist_ok=True)
    save(path, {"schemaVersion": int(schema),
                "midiSoundMap": [{"slot": s, "ampId": a, "presetId": p} for s, (a, p) in sorted(board.items())]})
    return 0


def cmd_variance(path):
    from PIL import Image, ImageStat

    try:
        im = Image.open(path)
        im.load()
    except Exception as e:  # noqa: BLE001 - any unreadable image is blank for this check
        print("unreadable: {}".format(e))
        return 1
    gray = im.convert("L")
    std = ImageStat.Stat(gray).stddev[0]
    colors = gray.getcolors(256)
    distinct = len(colors) if colors else 256
    print("{}x{} stddev {:.1f}, {} grey levels".format(im.width, im.height, std, distinct))
    return 0 if std >= 3.0 and distinct >= 16 and im.width >= 100 and im.height >= 100 else 1


def cmd_flat_png(path):
    from PIL import Image

    Image.new("RGB", (900, 600), (24, 17, 17)).save(path)
    return 0


def cmd_noise_png(path):
    from PIL import Image

    rng = random.Random(7)
    im = Image.new("RGB", (300, 200))
    im.putdata([(rng.randrange(256), rng.randrange(256), rng.randrange(256)) for _ in range(300 * 200)])
    im.save(path)
    return 0


def main_window(pid):
    import Quartz

    opts = Quartz.kCGWindowListOptionOnScreenOnly | Quartz.kCGWindowListExcludeDesktopElements
    best = None
    for w in Quartz.CGWindowListCopyWindowInfo(opts, Quartz.kCGNullWindowID) or []:
        if int(w.get("kCGWindowOwnerPID", -1)) != pid or int(w.get("kCGWindowLayer", 1)) != 0:
            continue
        b = w.get("kCGWindowBounds") or {}
        rect = (int(w.get("kCGWindowNumber")), float(b.get("X", 0)), float(b.get("Y", 0)), float(b.get("Width", 0)),
                float(b.get("Height", 0)))
        if rect[3] >= 300 and rect[4] >= 200 and (best is None or rect[3] * rect[4] > best[3] * best[4]):
            best = rect
    return best


def cmd_window(pid):
    w = main_window(int(pid))
    if not w:
        print("no on-screen window for pid " + pid)
        return 1
    print("{} {:.0f} {:.0f} {:.0f} {:.0f}".format(*w))
    return 0


def post_click(x, y):
    import Quartz

    pt = Quartz.CGPointMake(x, y)
    for kind in (Quartz.kCGEventMouseMoved, Quartz.kCGEventLeftMouseDown, Quartz.kCGEventLeftMouseUp):
        ev = Quartz.CGEventCreateMouseEvent(None, kind, pt, Quartz.kCGMouseButtonLeft)
        Quartz.CGEventPost(Quartz.kCGHIDEventTap, ev)
        time.sleep(0.08)


def activate(pid):
    try:
        import AppKit

        app = AppKit.NSRunningApplication.runningApplicationWithProcessIdentifier_(pid)
        if app is not None:
            app.activateWithOptions_(AppKit.NSApplicationActivateIgnoringOtherApps)
    except Exception:  # noqa: BLE001 - activation is best effort; the click activates too
        pass


def cmd_click_canvas(pid, cx, cy):
    pid = int(pid)
    w = main_window(pid)
    if not w:
        print("no on-screen window for pid {}".format(pid))
        return 1
    _, x, y, width, height = w
    scale = width / CANVAS_W
    top = y + height - CANVAS_H * scale
    gx, gy = x + float(cx) * scale, top + float(cy) * scale
    activate(pid)
    time.sleep(0.2)
    post_click(gx, gy)
    print("canvas ({}, {}) -> screen ({:.0f}, {:.0f})".format(cx, cy, gx, gy))
    return 0


def cmd_key(pid, char):
    import Quartz

    codes = {"p": 35}
    activate(int(pid))
    time.sleep(0.2)
    for down in (True, False):
        ev = Quartz.CGEventCreateKeyboardEvent(None, codes[char], down)
        Quartz.CGEventPost(Quartz.kCGHIDEventTap, ev)
        time.sleep(0.08)
    print("key {}".format(char))
    return 0


def cmd_screen_access():
    import Quartz

    try:
        print("granted" if Quartz.CGPreflightScreenCaptureAccess() else "denied")
    except AttributeError:
        print("unknown")
    return 0


def cmd_midi_serve(name, ready):
    """Own a virtual CoreMIDI source for the whole MIDI scenario and send what stdin asks:
    'send <b0> <b1> [b2]', 'repeat <n> <b0> [b1 b2]', 'quit'."""
    import rtmidi

    out = rtmidi.MidiOut()
    out.open_virtual_port(name)
    time.sleep(1.0)
    probe = rtmidi.MidiIn()
    ports = probe.get_ports()
    del probe
    seen = [p for p in ports if p == name] or [p for p in ports if name in p]
    with open(ready + ".tmp", "w", encoding="utf-8") as f:
        f.write((seen[0] if seen else "") + "\n")
        f.write("\n".join(ports) + "\n")
    os.replace(ready + ".tmp", ready)
    print("virtual source '{}' open; MIDI inputs: {}".format(name, ports), flush=True)
    for line in iter(sys.stdin.readline, ""):
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "quit":
            break
        if parts[0] == "send":
            out.send_message([int(x, 0) for x in parts[1:]])
        elif parts[0] == "repeat":
            msg = [int(x, 0) for x in parts[2:]]
            for _ in range(int(parts[1])):
                out.send_message(msg)
        print("sent " + line.strip(), flush=True)
    out.close_port()
    return 0


COMMANDS = {
    "json-get": cmd_json_get,
    "json-set": cmd_json_set,
    "parses": cmd_parses,
    "has-key": cmd_has_key,
    "mapkey": cmd_mapkey,
    "clear-map": cmd_clear_map,
    "move-slot": cmd_move_slot,
    "rewind-v2": cmd_rewind_v2,
    "measurable": cmd_measurable,
    "calibrated": cmd_calibrated,
    "count": cmd_count,
    "ir-json": cmd_ir_json,
    "lost": cmd_lost,
    "seed-board": cmd_seed_board,
    "variance": cmd_variance,
    "flat-png": cmd_flat_png,
    "noise-png": cmd_noise_png,
    "window": cmd_window,
    "click-canvas": cmd_click_canvas,
    "key": cmd_key,
    "screen-access": cmd_screen_access,
    "midi-serve": cmd_midi_serve,
}

if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1] not in COMMANDS:
        print("usage: e2e_standalone_mac.py <{}> ...".format("|".join(COMMANDS)), file=sys.stderr)
        sys.exit(2)
    sys.exit(COMMANDS[sys.argv[1]](*sys.argv[2:]))
