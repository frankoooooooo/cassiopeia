#!/bin/sh
# Exports rest, playing and c<N>-000/050/100 frames of an engine from its real display():
# reference/frames/export.sh schedar  → reference/frames/schedar/*.svg
set -e
cd "$(dirname "$0")/../.."
E=$1; O=reference/frames/$E; mkdir -p $O
c++ -std=c++17 -O1 -DENGINE=$E -I ../open-xy-staging/sdk/include reference/frames/dump.cpp -o $O/d
python3 - "$E" "$O" <<'PY'
import json, subprocess, sys
e, o = sys.argv[1:]
d = json.load(open(f"{e}/definition.json"))
ps = d["parameters"]
def caption(i, v):
    p = ps[i]
    if p["mapping"]["kind"] == "enum":
        L = p["mapping"]["labels"]; return L[min(len(L) - 1, int(v * len(L)))]
    if p["display"] == "bipolar": return "%+d" % round((2 * v - 1) * 100)
    return str(round(v * 100))
def run(name, mode, vals):
    hdr = []
    for i, p in enumerate(ps): hdr += [p["label"].lower(), caption(i, vals[i])]
    subprocess.run([f"{o}/d", f"{o}/{name}.txt", mode] + [str(v) for v in vals], check=True)
    subprocess.run(["python3", "reference/frames/compose.py", e, f"{o}/{name}.txt", f"{o}/{name}.svg"] + hdr, check=True)
base = [p["default"] for p in ps]
run("rest", "rest", base); run("playing", "playing", base)
for i in range(4):
    for v, tag in ((0, "000"), (0.5, "050"), (1, "100")):
        vals = list(base); vals[i] = v if ps[i]["mapping"]["kind"] != "enum" else min(0.999, v)
        run(f"c{i + 1}-{tag}", "rest", vals)
PY
rm $O/d $O/*.txt
