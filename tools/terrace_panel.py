#!/usr/bin/env python3
"""Writes a cell-terrace panel.svg.fragment: cols x rows oblique cell tops (nodes, back row first) under
the stock four-control header, and 16 greys. Geometry must match the engine's display.h / display.cpp.
python3 tools/terrace_panel.py schedar 16 9 > schedar/panel.svg.fragment
python3 tools/terrace_panel.py caph 14 8 split > caph/panel.svg.fragment   (two half tops per cell: A then B)
"""
import sys
mid, cols, rows = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
split = len(sys.argv) > 4 and sys.argv[4] == 'split'
LEFT, SPAN, DX, DY, BASE = 14, 360, 11, 15, 180
TOP = 'M-15.5 3 L-4.5 -3 L15.5 -3 L4.5 3 Z'  # a 20 px top, 11 px oblique over 6 px, centred on its origin
HALF = 'M-10 3 L1 -3 L10 -3 L-1 3 Z'        # a 9 px half top, same slant
GREYS = ['#%02X%02X%02X' % ((round(30 + 13.5 * k),) * 3) for k in range(16)]
out = [f'<g id="{mid}_panel">', '<!-- terrace cells, back row first; transform before d, as paths draw on the device -->']
per = 2 if split else 1
for n in range(cols * rows * per):
    c, h = n // per, n % per
    j, i = rows - 1 - c // cols, c % cols
    x, y = LEFT + (i + 0.5) * SPAN / cols + j * DX + ((h * 11 - 5.5) if split else 0), BASE - j * DY
    out.append(f'  <path id="{mid}_node_{n}" transform="matrix(1 0 0 1 {x:.1f} {y:.1f})" d="{HALF if split else TOP}" fill="{GREYS[6]}" stroke="{GREYS[6]}" stroke-width="0.5"/>')
out += [f'  <circle id="{mid}_palette_{k}" cx="640" cy="0" r="1" fill="{g}" stroke="{g}" stroke-width="1"/>' for k, g in enumerate(GREYS)]
out.append('</g>')
print('\n'.join(out))
