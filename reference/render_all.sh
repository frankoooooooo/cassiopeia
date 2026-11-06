#!/bin/sh
# Renders every preset (phrase) and every enum choice of control 3 (tour) of one engine: render_all.sh schedar
set -e
cd "$(dirname "$0")/.."
E=$1; R=reference/renders/$E; mkdir -p $R
c++ -std=c++17 -O2 -DENGINE=$E -I ../open-xy-staging/sdk/include reference/render.cpp -o $R/r
python3 - "$E" "$R" <<'PY'
import json, re, subprocess, sys
e, r = sys.argv[1:]
d = json.load(open(f"{e}/definition.json"))
ids = [p["id"] for p in d["parameters"]]
for name, vals in d["presets"].items():
    slug = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
    subprocess.run([f"{r}/r", f"{r}/preset-{slug}.wav", "phrase"] + [str(vals[i]) for i in ids], check=True)
enum = next((i for i, p in enumerate(d["parameters"]) if p["mapping"]["kind"] == "enum"), None)
base = [p["default"] for p in d["parameters"]]
base[3] = 0.5  # Sweep centred: the tour hears Amount alone
labels = d["parameters"][enum]["mapping"]["labels"] if enum is not None else ["all"]
for k, label in enumerate(labels):
    v = list(base)
    if enum is not None: v[enum] = (k + 0.5) / len(labels)
    subprocess.run([f"{r}/r", f"{r}/tour-{k}-{label.lower()}.wav", "tour"] + [str(x) for x in v], check=True)
PY
rm $R/r
