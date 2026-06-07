import json, sys
p = sys.argv[1]
d = json.load(open(p))
arch = d.get("architecture")
print("architecture:", arch)
cfg = d.get("config", {})
subs = cfg.get("submodels")
if subs:
    for i, s in enumerate(subs):
        sc = s.get("config", {})
        print(f"  submodel[{i}] channels={sc.get('channels')} max_value={s.get('max_value')}")
else:
    print("  (no submodels; keys:", list(cfg.keys())[:8], ")")
md = d.get("metadata", {})
print("loudness:", md.get("loudness"))
