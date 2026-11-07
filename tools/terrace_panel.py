#!/usr/bin/env python3
"""Writes a cell-terrace panel.svg.fragment: cols x rows oblique cell tops (nodes, back row first) under
the stock four-control header, and 16 greys. Geometry must match the engine's display.h / display.cpp.
python3 tools/terrace_panel.py schedar 16 9 > schedar/panel.svg.fragment
python3 tools/terrace_panel.py caph 14 8 split > caph/panel.svg.fragment   (two half tops per cell: A then B)
python3 tools/terrace_panel.py ruchbah 14 9 > ruchbah/panel.svg.fragment
python3 tools/terrace_panel.py achird 16 8 riser > achird/panel.svg.fragment   (a riser, then a top)
python3 tools/terrace_panel.py navi 36 7 > navi/panel.svg.fragment   (252 small tops for the banded stacks)
"""
import sys
mid, cols, rows = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
mode = sys.argv[4] if len(sys.argv) > 4 else ''
split, pillar, riser, slat = mode == 'split', mode == 'pillar', mode == 'riser', mode == 'slat'
LEFT, SPAN, DX, DY, BASE = 14, 360, 11, 15, 180
TOP = 'M-15.5 3 L-4.5 -3 L15.5 -3 L4.5 3 Z'  # a 20 px top, 11 px oblique over 6 px, centred on its origin
HALF = 'M-10 3 L1 -3 L10 -3 L-1 3 Z'        # a 9 px half top, same slant
BIG = 'M-19.4 3.8 L-5.6 -3.8 L19.4 -3.8 L5.6 3.8 Z'  # a 25 px top for pillars
FACE = 'M-19.4 3.8 H5.6 V96 H-19.4 Z'              # its front face, long enough to reach behind the next row
RISER = 'M-0.6 0 H0.6 V-32 H-0.6 Z'                # a 32 px riser up from its origin, scaled to each step
SLAT = 'M-8 -1.1 H8 V1.1 H-8 Z'                     # a 16 x 2.2 px slat, centred, turned to the slope
GREYS = ['#%02X%02X%02X' % ((round(30 + 13.5 * k),) * 3) for k in range(16)]
out = [f'<g id="{mid}_panel">', '<!-- terrace cells, back row first; transform before d, as paths draw on the device -->']
per = 2 if split or pillar or riser else 1
for n in range(cols * rows * per):
    c, h = n // per, n % per
    j, i = rows - 1 - c // cols, c % cols
    x, y = LEFT + (i + 0.5) * SPAN / cols + j * DX + ((h * 11 - 5.5) if split else 0), BASE - j * DY
    d = HALF if split else (FACE if h == 0 else BIG) if pillar else (RISER if h == 0 else TOP) if riser else SLAT if slat else TOP
    g = GREYS[2] if pillar and h == 0 else GREYS[12] if riser and h == 0 else GREYS[6]
    out.append(f'  <path id="{mid}_node_{n}" transform="matrix(1 0 0 1 {x:.1f} {y:.1f})" d="{d}" fill="{g}" stroke="{g}" stroke-width="0.5"/>')
out += [f'  <circle id="{mid}_palette_{k}" cx="640" cy="0" r="1" fill="{g}" stroke="{g}" stroke-width="1"/>' for k, g in enumerate(GREYS)]
out.append('</g>')
print('\n'.join(out))
