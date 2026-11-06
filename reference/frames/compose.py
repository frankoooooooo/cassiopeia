#!/usr/bin/env python3
"""Applies a dumped frame to the engine's panel.svg.fragment: each node keeps its authored shape (d) and
takes the frame's transform and palette grey. Writes a 480 x 220 SVG with a stock-style header.
compose.py <engine folder> <frame.txt> <out.svg> label0 value0 … label3 value3
"""
import re, sys
folder, frame, out = sys.argv[1:4]
hdr = sys.argv[4:12]
frag = open(f"{folder}/panel.svg.fragment").read()
shapes = dict(re.findall(r'id="\w+_node_(\d+)"[^>]*? d="([^"]+)"', frag))
greys = [g for _, g in sorted((int(i), g) for i, g in re.findall(r'id="\w+_palette_(\d+)"[^>]*? fill="([^"]+)"', frag))]
over = {}
nodes = []
for line in open(frame):
    p = line.split()
    if p[0] == "node": nodes.append((p[1], float(p[2]), float(p[3]), float(p[4]), int(p[5])))
    elif p[0] == "header" and len(p) > 2: over[int(p[1])] = p[2]
steps = ["#000000", "#323232", "#4B4B4B", "#646464", "#7D7D7D", "#969696", "#AFAFAF", "#CDCDCD"]
o = ['<svg xmlns="http://www.w3.org/2000/svg" width="480" height="220" viewBox="0 0 480 220">', '<rect width="480" height="220" fill="#000"/>']
o += [f'<rect x="{i * 60}" y="0" width="60" height="20" fill="{c}"/>' for i, c in enumerate(steps)]
for i in range(4):
    label, value = hdr[2 * i], over.get(i, hdr[2 * i + 1])
    o.append(f'<text x="{5 + 120 * i}" y="13" font-size="10" fill="#CBCBCB" font-family="UniversNextPro-Light, Helvetica">{label}</text>')
    o.append(f'<text x="{65 + 120 * i}" y="17" font-size="16" fill="#000" font-family="UniversNextPro-Light, Helvetica">{value}</text>')
for n, x, y, s, shade in nodes:
    if x >= 640: continue
    g = greys[shade]
    o.append(f'<path transform="matrix({s} 0 0 {s} {x} {y})" d="{shapes[n]}" fill="{g}" stroke="{g}" stroke-width="0.5"/>')
o.append("</svg>")
open(out, "w").write("\n".join(o) + "\n")
