#!/usr/bin/env python3
"""Writes <engine>/presets/<name>.json (device presets, version 4) from each definition.json's named
presets, and points machine.json at them. The envelope is stock's init amp envelope unless machine.json
sets presetDefaults.amp. Usage: python3 tools/make_presets.py schedar caph navi ruchbah segin achird
"""
import json, re, sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
MOD = {s: {'amount': 16384, 'target': 0} for s in ['aftertouch', 'modwheel', 'pitchbend', 'velocity']}
for mid in sys.argv[1:]:
    d = json.loads((root / mid / 'definition.json').read_text())
    machine_path = root / mid / 'machine.json'
    machine = json.loads(machine_path.read_text())
    amp = {'attack': 128, 'decay': 8192, 'release': 32767, 'sustain': 32767, **machine.get('presetDefaults', {}).get('amp', {})}
    order = [p['id'] for p in d['parameters']]
    out = root / mid / 'presets'
    out.mkdir(exist_ok=True)
    for old in out.glob('*.json'): old.unlink()
    assert 1 <= len(d['presets']) <= 16
    for name, values in d['presets'].items():
        slug = re.sub(r'[^a-z0-9]+', '-', name.lower()).strip('-')
        assert re.fullmatch(r'[a-z0-9_-]{1,24}', slug), slug
        params = [round(values[k] * 32767) for k in order] + [0] * (8 - len(order))
        assert all(0 <= p <= 32767 for p in params)
        preset = {
            'engine': {'bendrange': 8191, 'highpass': 0, 'modulation': MOD, 'playmode': 'poly',
                       'portamento.amount': 0, 'portamento.type': 0, 'transpose': 0, 'tuning.root': 0,
                       'tuning.scale': 0, 'velocity.sensitivity': 0, 'volume': 16384, 'width': 0, 'params': params},
            'envelope': {'amp': amp, 'filter': {'attack': 0, 'decay': 0, 'release': 0, 'sustain': 32767}},
            'fx': {'active': False, 'params': [0, 0, 0, 0, 0, 32767, 0, 0], 'type': 'svf'},
            'lfo': {'active': False, 'params': [0] * 8, 'type': 'value'},
            'octave': 0, 'platform': 'OP-XY', 'regions': [], 'version': 4, 'type': d['id'],
        }
        (out / f'{slug}.json').write_text(json.dumps(preset, indent=2) + '\n')
    if machine.get('presets') != 'presets':
        machine['presets'] = 'presets'
        machine_path.write_text(json.dumps(machine, indent=2, ensure_ascii=False) + '\n')
    print(mid, len(d['presets']), 'presets')
