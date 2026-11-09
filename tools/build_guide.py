#!/usr/bin/env python3
"""Builds docs/guide.html: the illustrated field guide to the six engines.

It bundles, into one self-contained page:
  - each engine's motion take (reference/frames/takes/<engine>.json, from takes.py) and its panel's authored
    node shapes and 16 greys (panel.svg.fragment), so the page replays the real display() frame by frame;
  - the preset and tour renders (reference/renders/<engine>/*.wav, from render_all.sh) as small MP3s;
  - parameter labels and presets from each definition.json.
Both inputs are generated and git-ignored, so run takes.py and render_all.sh first.

  python3 tools/build_guide.py [--out docs/guide.html] [--body out.html]

--body also writes the page without its <!doctype>/<head> wrapper (for hosts that add their own).
"""
import argparse, base64, json, re, subprocess, tempfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent
engines = ["schedar", "caph", "navi", "ruchbah", "segin", "achird"]
tours = {"schedar": ["0-saw", "1-sqr", "2-pulse", "3-dbl", "4-sawp", "5-res1", "6-res2", "7-res3"],
         "ruchbah": ["0-pluck", "1-hit", "2-noise", "3-tri"]}

ap = argparse.ArgumentParser()
ap.add_argument("--out", default=str(root / "docs/guide.html"))
ap.add_argument("--body")
ap.add_argument("--kbps", default="48k")
a = ap.parse_args()


def mp3(path):
    with tempfile.TemporaryDirectory() as tmp:
        o = Path(tmp) / "a.mp3"
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(path), "-ac", "1", "-b:a", a.kbps, str(o)], check=True)
        return "data:audio/mpeg;base64," + base64.b64encode(o.read_bytes()).decode()


data = {}
for e in engines:
    d = json.load(open(root / e / "definition.json"))
    frag = open(root / e / "panel.svg.fragment").read()
    shapes = dict(re.findall(r'id="\w+_node_(\d+)"[^>]*? d="([^"]+)"', frag))
    authored_fill = dict(re.findall(r'id="\w+_node_(\d+)"[^>]*? fill="([^"]+)"', frag))
    greys = [g for _, g in sorted((int(i), g) for i, g in re.findall(r'id="\w+_palette_(\d+)"[^>]*? fill="([^"]+)"', frag))]
    n = len(shapes)
    take = json.load(open(root / "reference/frames/takes" / f"{e}.json"))
    frames = [{"c": [round(v, 3) for v in f["c"]], "n": f["n"], "h": f["h"]} for f in take]
    presets = []
    for name, vals in d["presets"].items():
        wav = root / "reference/renders" / e / f"preset-{name.lower()}.wav"
        presets.append({"name": name, "v": [vals[p["id"]] for p in d["parameters"]],
                        "audio": mp3(wav) if wav.exists() else None})
    data[e] = {
        "name": d["name"],
        "params": [{"id": p["id"], "label": p["label"], "display": p["display"], "mapping": p["mapping"],
                    "default": p["default"]} for p in d["parameters"]],
        "shapes": [shapes[str(i)] for i in range(n)],
        "keepFill": [authored_fill.get(str(i), "none") != "none" for i in range(n)],
        "greys": greys,
        "take": frames,
        "presets": presets,
        "tours": [{"name": t.split("-", 1)[1], "audio": mp3(root / "reference/renders" / e / f"tour-{t}.wav")}
                  for t in tours.get(e, []) if (root / "reference/renders" / e / f"tour-{t}.wav").exists()],
    }
    print(e, n, "nodes", len(frames), "frames", sum(1 for p in presets if p["audio"]), "preset renders")

template = open(root / "tools/guide_template.html").read()
body = template.replace("/*DATA*/null", json.dumps(data, separators=(",", ":")))
out = Path(a.out)
out.parent.mkdir(parents=True, exist_ok=True)
head, rest = body.split("<!--/head-->", 1)
out.write_text('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
               '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n'
               + head + '</head>\n<body>\n' + rest + "\n</body>\n</html>\n")
print("wrote", out, f"{out.stat().st_size / 1e6:.1f} MB")
if a.body:
    Path(a.body).write_text(body)
