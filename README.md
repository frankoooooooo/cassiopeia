# Cassiopeia

Six synth engines for the OP-XY, each named after a star of Cassiopeia. It began with **Schedar**, a
phase-distortion voice after the Casio CZ-101 (1984), and grew into a family of six oscillators, each with its
own way of shaping sound, from brassy CZ saws to beating analogue pairs, singing formants, a struck ring of
springs, a 1978 voice synthesizer and chiptune-edged Walsh steps.

Every engine has four knobs: a brightness control, two of its own, and **Sweep**, a CZ-style envelope that
makes each note start brighter or darker and settle. Every engine also draws its own animated screen.

Built with the [Open-XY SDK](https://github.com/etcetc000/open-xy).

## The engines

| Engine | Description | Knobs |
|---|---|---|
| **Schedar** | Phase distortion after the Casio CZ: simple and clean, eight waves from round sines to brassy saws and resonant sweeps, shaped by Warp, Width and Sweep. | Warp · Width · Wave · Sweep |
| **Caph** | Virtual analogue: a gritty pair of detuned oscillators that morph from triangle to pulse to saw and beat against each other, for rich leads and complex pads. | Shape · Width · Detune · Sweep |
| **Navi** | Vector phaseshaping: tunable formants, from vowel-like tones to hard-sync-style leads, shaped by Formant, Inflect and Range. | Formant · Inflect · Range · Sweep |
| **Ruchbah** | Scanned synthesis: each note strikes a ring of 32 masses that moves and settles while a scanner reads it as a living wavetable, with the family's Sweep. | Stiff · Damping · Excite · Sweep |
| **Segin** | VOSIM: each cycle fires a burst of fading sin² pulses at a fixed formant, so every note speaks the same vowel, with the family's bipolar Sweep. | Formant · Decay · Pulses · Sweep |
| **Achird** | Walsh functions: fifteen square-step waves summed into a 16-step, chip-edged cycle, tilted dark to harsh and folded, with the family's bipolar Sweep. | Tilt · Balance · Fold · Sweep |

**Sweep**, on every engine: above centre, each note starts brighter than the first knob and settles back over
about half a second; below centre, it starts darker and opens up; at the centre, nothing moves.

Each engine comes with six presets.

## Screens

Every screen is a field of small floating squares seen at an angle, the "cell terrace": across is the wave's
cycle, going back is the last moments, and grey follows height. Each engine bends it to show what it does:
Schedar's columns crowd where its phase rushes, Caph's cells split as its pair beats, Navi's piles turn solid or
hollow with each formant cycle, Ruchbah's squares swell as its ring moves, Segin's pulses rise as islands, and
Achird's steps stand as blocks. Screens glide rather than step, and go still when nothing sounds.

## Installing

Once in the Open-XY catalog, the engines can be added to a firmware build with the Open-XY builder. To build
from this repository, see the SDK's [build guide](https://github.com/etcetc000/open-xy/blob/staging/docs/build.md).

## Development

Each folder is a self-contained machine. `tools/sync_common.sh` copies the shared `schedar/cz_common.h` into the
others; CI checks that the copies match. Tools:
- `reference/render_all.sh <folder>`: listening renders of every preset and a tour.
- `reference/frames/export.sh <folder>`: the screen's frames from its real `display()`.
- `reference/frames/motion.cpp`, `takes.py`, `busy.py`: scripted takes through `display()` as frame deltas, and how busy each screen is (px moved per square per frame, updates, settle time).
- `reference/frames/check.sh <folder>`: operator-design's checks.
- `tools/screen_sweep.sh`: every screen under the sanitizers at the controls' extremes, and a check that it comes to rest after a note (CI runs it).
- `tools/terrace_panel.py`: the terrace artwork.
- `tools/make_presets.py`: the device presets.

## Credits

Inspired by [PD Hybrid Synth](https://github.com/ChuckFleming/pd-hybrid-synth) by Chuck Fleming. Cassiopeia's
code is written from scratch from the published techniques: Casio CZ phase distortion; vector phaseshaping
(Kleimola, Lazzarini, Timoney and Välimäki, 2011); scanned synthesis (Verplank, Mathews and Shaw, 2000); VOSIM
(Kaegi and Tempelaars, 1978); and Walsh-function synthesis. None of PD Hybrid Synth's code is used. MIT licence.

Casio and CZ-101 are trademarks of Casio Computer Co., Ltd. Cassiopeia is not affiliated with or endorsed by
Casio.
