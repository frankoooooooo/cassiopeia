#!/usr/bin/env python3
"""Writes every engine's presets from tools/presets.json: <engine>/presets/<name>.json (device presets,
version 4) and the named presets in <engine>/definition.json (the browser preview's list), and points
machine.json at the presets folder.

tools/presets.json, per engine, a list of presets in friendly units:
  name, controls (the engine's controls, 0..1; an enum's choice i of n is (i + 0.5) / n),
  amp {attack, decay, release: seconds; sustain: 0..1},
  filter (optional: the track's SVF on) {cutoff, resonance, envelope (amount), keytrack: 0..1;
          attack, decay, release: seconds; sustain: 0..1},
  and optionally octave (−3..+2), mode ("poly" or "mono"), glide, velocity and volume (0..1; defaults
  0, poly, 0, 0 and 0.5, the stock init's).
Times use the OP-XY's square-law scale, value = 32767·√(t / 10 s), and release runs the other way (a
larger value is a shorter release), as stock reads it. Usage: python3 tools/make_presets.py [engine…]
"""
import json, math, re, sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
table = json.loads((root / 'tools/presets.json').read_text())
MOD = {s: {'amount': 16384, 'target': 0} for s in ['aftertouch', 'modwheel', 'pitchbend', 'velocity']}

def time(t):
    return round(32767 * math.sqrt(min(max(t, 0.0), 10.0) / 10.0))

def level(x):
    return round(min(max(x, 0.0), 1.0) * 32767)

def envelope(e):
    return {'attack': time(e['attack']), 'decay': time(e['decay']), 'release': 32767 - time(e['release']),
            'sustain': level(e['sustain'])}

for mid in sys.argv[1:] or list(table):
    definition_path = root / mid / 'definition.json'
    definition = json.loads(definition_path.read_text())
    ids = [p['id'] for p in definition['parameters']]
    presets = table[mid]
    assert 1 <= len(presets) <= 16, mid
    out = root / mid / 'presets'
    out.mkdir(exist_ok=True)
    for old in out.glob('*.json'):
        old.unlink()
    named = {}
    for p in presets:
        slug = re.sub(r'[^a-z0-9]+', '-', p['name'].lower()).strip('-')
        assert re.fullmatch(r'[a-z0-9_-]{1,24}', slug), slug
        controls = p['controls']
        assert len(controls) == len(ids) and all(0 <= c <= 1 for c in controls), p['name']
        named[p['name']] = dict(zip(ids, controls))
        f = p.get('filter')
        preset = {
            'engine': {'bendrange': 8191, 'highpass': 0, 'modulation': MOD, 'playmode': p.get('mode', 'poly'),
                       'portamento.amount': level(p.get('glide', 0)), 'portamento.type': 0, 'transpose': 0, 'tuning.root': 0,
                       'tuning.scale': 0, 'velocity.sensitivity': level(p.get('velocity', 0)),
                       'volume': level(p.get('volume', 0.5)), 'width': 0,
                       'params': [level(c) for c in controls] + [0] * (8 - len(controls))},
            'envelope': {'amp': envelope(p['amp']),
                         'filter': envelope(f) if f else {'attack': 0, 'decay': 0, 'release': 0, 'sustain': 32767}},
            'fx': ({'active': True, 'type': 'svf',
                    'params': [level(f['cutoff']), level(f['resonance']), level(f['envelope']), level(f['keytrack']), 0, 0, 0, 0]}
                   if f else {'active': False, 'params': [0, 0, 0, 0, 0, 32767, 0, 0], 'type': 'svf'}),
            'lfo': {'active': False, 'params': [0] * 8, 'type': 'value'},
            'octave': p.get('octave', 0), 'platform': 'OP-XY', 'regions': [], 'version': 4, 'type': definition['id'],
        }
        (out / f'{slug}.json').write_text(json.dumps(preset, indent=2) + '\n')
    definition['presets'] = named
    definition_path.write_text(json.dumps(definition, indent=2, ensure_ascii=False) + '\n')
    machine_path = root / mid / 'machine.json'
    machine = json.loads(machine_path.read_text())
    if machine.get('presets') != 'presets':
        machine['presets'] = 'presets'
        machine_path.write_text(json.dumps(machine, indent=2, ensure_ascii=False) + '\n')
    print(mid, len(presets), 'presets,', sum(1 for p in presets if 'filter' in p), 'with the filter on')
