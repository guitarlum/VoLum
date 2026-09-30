"""Bake the owner's Sounds Pack into rigs/factory-presets.json (ticket 15).

Usage: python .scratch/1.3.0-feedback/bake_factory_presets.py <pack.volumpack> [--pick factory:N=<preset name> ...]

Rules (owner, 2026-09-29): the Pack must give every one of the 15 factory amps a sound, and every sound
must use bundled content only. Otherwise nothing is written and the problems are listed.
"""
import json
import sys
import zipfile

AMP_COUNT = 15
CUSTOM_PEDAL_INDEX_BASE = 64  # content::kCustomPedalIndexBase
OUT = "rigs/factory-presets.json"


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    picks = {}
    for arg in argv[2:]:
        if arg.startswith("--pick="):
            arg = arg[len("--pick=") :]
        if "=" in arg and arg.startswith("factory:"):
            k, v = arg.split("=", 1)
            picks[k] = v
    with zipfile.ZipFile(argv[1]) as z:
        library = json.loads(z.read("library.json").decode("utf-8"))
        manifest = json.loads(z.read("manifest.json").decode("utf-8"))
    print("pack job:", manifest.get("job"), "contract", manifest.get("contractVersion"))
    banks = library.get("presetBanks", {})
    problems, baked = [], {}
    for idx in range(AMP_COUNT):
        owner = f"factory:{idx}"
        presets = banks.get(owner, [])
        if owner in picks:
            presets = [p for p in presets if p.get("name") == picks[owner]]
        if not presets:
            problems.append(f"{owner}: no sound in the Pack")
            continue
        if len(presets) > 1:
            names = ", ".join(repr(p.get("name")) for p in presets)
            problems.append(f"{owner}: {len(presets)} sounds ({names}); pass --pick={owner}=<name>")
            continue
        p = presets[0]
        s = p.get("settings", {})
        custom = [k for k in ("activeIrId", "supportActiveIrId", "supportCustomId") if s.get(k)]
        for slot in ("preNam1", "preNam2"):
            if s.get(slot + "Active") and int(s.get(slot + "Capture", 0)) >= CUSTOM_PEDAL_INDEX_BASE:
                custom.append(slot + "Capture (custom pedal)")
        if custom:
            problems.append(f"{owner} {p.get('name')!r}: uses non-bundled content: {', '.join(custom)}")
            continue
        baked[f"{owner}:v1"] = {"name": p.get("name", "").strip(), "settings": s}
    others = sorted(k for k in banks if not k.startswith("factory:"))
    if others:
        print("ignored (custom-amp owners):", ", ".join(others))
    if problems:
        print("NOT WRITTEN:")
        for line in problems:
            print("  -", line)
        return 1
    lines = ["{"]
    keys = [f"factory:{i}:v1" for i in range(AMP_COUNT)]
    for i, k in enumerate(keys):
        entry = json.dumps(baked[k], ensure_ascii=False, sort_keys=True)
        lines.append(f'  "{k}": {entry}' + ("," if i + 1 < len(keys) else ""))
    lines.append("}")
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    for k in keys:
        print(f"  {k}: {baked[k]['name']}")
    print("wrote", OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
