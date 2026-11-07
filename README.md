# cassiopeia

A family of six [Open-XY](https://github.com/etcetc000/open-xy) synth engines for the OP-XY, one per
oscillator of [PD Hybrid Synth](https://github.com/ChuckFleming/pd-hybrid-synth), each named after a star of Cassiopeia:

| star | engine | 1st (Amount) | 2nd | 3rd | 4th | screen: the cell terrace as… |
|---|---|---|---|---|---|---|
| Schedar `schedar` | Phase distortion, 8 CZ waves | Amount (DCW) | Width (cycle gap) | Wave | Sweep | warped columns, spaced by the phase map |
| Caph `caph` | Analog pair, triangle → pulse → saw, polyBLEP | Shape | Width (duty, apex) | Detune (cents) | Sweep | stacks: piles as tall as the pair's sum, rising and falling with the beat |
| Navi `navi` | Vector phaseshaping | Formant | Inflect | Range | Sweep | cycle bands: cells filled or outlined by each cosine cycle the bend packs in |
| Ruchbah `ruchbah` | Scanned synthesis, 32-mass ring | Stiff | Damping | Excite | Sweep | masses that swell as they move, on the screen's own copy of the ring |
| Segin `segin` | VOSIM | Formant (Hz) | Decay | Pulses | Sweep | burst islands over an outlined floor |
| Achird `achird` | Walsh functions, 16-step cycle | Tilt | Balance | Fold | Sweep | stepped blocks with lit risers |

Sweep is the family's bipolar DCW envelope on the first control: above centre a bright strike settles back,
below centre a dark strike opens up (about 0.5 s). Every screen is 16 greys, at most 256 nodes, under the
stock header; rows going back are recent history, and nothing redraws at rest.

**Clean-room.** PD Hybrid Synth is GPLv3, and its code is not used here. These engines re-implement the
published techniques in fixed point for the Blackfin: Casio CZ phase distortion, Kleimola et al.'s vector
phaseshaping (2011), Verplank/Shaw/Mathews scanned synthesis (2000), Kaegi and Tempelaars' VOSIM (1978) and
Walsh-function synthesis. The VST served only as a reference for behaviour and sound. MIT licence.

Each folder is a self-contained machine. `tools/sync_common.sh` copies the shared `schedar/cz_common.h` into the
others; CI checks that the copies match. Tools:
- `reference/render_all.sh <folder>`: listening renders of every preset and a tour.
- `reference/frames/export.sh <folder>`: the screen's frames from its real `display()`.
- `reference/frames/motion.cpp`: a scripted five-second take through `display()`, as frame deltas for a moving review page.
- `reference/frames/check.sh <folder>`: operator-design's checks.
- `tools/terrace_panel.py`: the terrace artwork.
- `tools/make_presets.py`: the device presets.
