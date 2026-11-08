#!/usr/bin/env python3
"""How busy a motion take is, while the note sounds (frames 30..140):
  updates   share of nodes rewritten per frame (any change: move, shade, show/hide)
  motion    mean px a visible node moves per frame (hide/show excluded)
  row-jumps share of frames where over 40% of visible nodes move a pixel or more at once
  quiet     how long after release until a frame changes nothing
  python3 reference/frames/busy.py take.json…          report
  python3 reference/frames/busy.py --check-rest take…  fail unless each take's last 30 frames change nothing
"""
import json, sys

def stats(path):
    frames = json.load(open(path))
    pos, moves, bursts, counted, quiet_at, updates = {}, 0.0, 0, 0, None, 0
    for f, fr in enumerate(frames):
        moved = 0
        if 30 <= f < 141 and f: updates += len(fr['n'])
        for i, x, y, sc, *_ in fr['n']:
            hidden = x >= 2400
            if i in pos and not hidden and pos[i] is not None and 30 <= f < 141:
                px, py = pos[i]
                d = (abs(x - px) + abs(y - py)) / 4
                moves += d
                moved += d >= 1
            pos[i] = None if hidden else (x, y)
        visible = sum(1 for v in pos.values() if v)
        if 30 <= f < 141:
            counted += 1
            bursts += visible and moved > 0.4 * visible
        if f >= 141 and quiet_at is None and not fr['n']: quiet_at = f - 141
    visible = max(1, sum(1 for v in pos.values() if v))
    return moves / visible / counted, bursts / counted, quiet_at, updates / max(1, len(pos)) / counted, frames

args = sys.argv[1:]
check = args[:1] == ['--check-rest']
bad = 0
for p in args[1:] if check else args:
    m, b, q, u, frames = stats(p)
    name = p.split('/')[-1]
    if check:
        busy_tail = sum(1 for fr in frames[-30:] if fr['n'])
        print(f"{name}: {'rests' if not busy_tail else f'STILL REDRAWING in {busy_tail} of its last 30 frames'}")
        bad += busy_tail > 0
    else:
        print(f"{name:16s} updates {100*u:5.1f}% of nodes/frame  motion {m:5.2f} px  row-jumps {100*b:4.1f}%  quiet {'never' if q is None else f'{q/30:.2f} s after release'}")
sys.exit(1 if bad else 0)
