#!/usr/bin/env python3
"""Records each engine's motion take: motion.cpp's scripted 240 frames through the real display() (rest,
a held note with Sweep raised, control 1 then control 2 sweeping, release at 4.7 s, then 5 s of
settling), as frame deltas in <out>/<engine>.json.
  python3 reference/frames/takes.py [--out DIR] [--sdk INCLUDE] [--cxx CXX] engine…
"""
import argparse, json, subprocess, tempfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent.parent
ap = argparse.ArgumentParser()
ap.add_argument('engines', nargs='+')
ap.add_argument('--out', default='reference/frames/takes')
ap.add_argument('--sdk', default=str(root.parent / 'open-xy-staging/sdk/include'))
ap.add_argument('--cxx', default='c++')
a = ap.parse_args()
out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory() as tmp:
    for e in a.engines:
        ps = json.load(open(root / e / 'definition.json'))['parameters']
        base = [p['default'] for p in ps]
        to1 = 0.05 if base[0] >= 0.8 else 1.0                       # control 1 to its far end
        to2 = 0.999 if ps[1]['mapping']['kind'] == 'enum' else (0.0 if base[1] >= 0.5 else 1.0)
        exe = Path(tmp) / f'm_{e}'
        subprocess.run([a.cxx, '-std=c++17', '-O1', '-iquote', str(root / e), '-I', a.sdk, f'-DENGINE={e}',
                        str(root / 'reference/frames/motion.cpp'), '-o', str(exe)], check=True)
        subprocess.run([str(exe), str(out / f'{e}.json'), *map(str, base[:3]), '0', str(max(base[3], 0.85)),
                        str(to1), str(to2), '300'], check=True)
