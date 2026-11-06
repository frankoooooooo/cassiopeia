# cz-101

A family of six [Open-XY](https://github.com/etcetc000/open-xy) synth engines for the OP-XY, one per
oscillator of [PD Hybrid Synth](https://github.com/ChuckFleming/pd-hybrid-synth):

| folder | engine | Amount | Width | 3rd | Sweep |
|---|---|---|---|---|---|
| `cz101` | Phase distortion, 8 CZ waves | DCW depth | cycle gap | Wave | DCW envelope |
| `czanalog` | polyBLEP saw / square / triangle / pulse | | | | |
| `czvector` | Vector phaseshaping | | | | |
| `czscan` | Scanned synthesis | | | | |
| `czvosim` | VOSIM | | | | |
| `czwalsh` | Walsh functions | | | | |

**Clean-room.** PD Hybrid Synth is GPLv3, and its code is not used here. These engines re-implement the
published techniques in fixed point for the Blackfin: Casio CZ phase distortion, Kleimola et al.'s vector
phaseshaping (2011), Verplank/Shaw/Mathews scanned synthesis (2000), Kaegi and Tempelaars' VOSIM (1978) and
Walsh-function synthesis. The VST served only as a reference for behaviour and sound. MIT licence.

Each folder is a self-contained machine. `tools/sync_common.sh` copies the shared `cz101/cz_common.h` into the
others; CI checks that the copies match. Listening renders: `reference/render_all.sh <folder>`.
