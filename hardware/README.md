# Koloured-Verb — hardware

Three boards, one 12 HP module.

```
  panel-board      12 HP FR4 panel: cutouts and baked labels
       │  pots / jacks / LEDs poke through
  control-board    panel parts on F.Cu, carrier headers on B.Cu
       │  ZZ1-5 / ZZ7 / ZZ12 + J6 / J9 / J11 plug down into…
  core-board       Daisy Patch SM host, power, I/O expansion
       │  4× 5×2 sockets host…
  Daisy Patch SM   Electrosmith submodule (bought, not fabricated)
```

| Folder | Board | Size | Assembly |
|---|---|---|---|
| [`core-board/`](core-board/) | carrier | 59.75 × 109.65 mm | 45 SMD by JLCPCB, 12 through-hole by hand |
| [`control-board/`](control-board/) | pots, jacks, buttons, LEDs | 59.75 × 109.65 mm | all by hand; one SMD RGB LED |
| [`panel-board/`](panel-board/) | 12 HP panel | 60.75 × 128.65 mm | none |

All three are 2-layer, 1.6 mm FR4. Each folder holds its KiCad sources, a
`<board>.bom.csv`, and `fab/` with the gerber zip and the loose set it was built
from. The core board also carries `fab/*-jlcpcb-bom.csv` and `-cpl.csv` for
assembly.

`ZZ10` on the control board is **DNP** — the core board carries no microSD, so
that header has no mating socket.

## Gerbers

```sh
kicad-cli pcb export gerbers --no-protel-ext \
  --layers F.Cu,B.Cu,F.Mask,B.Mask,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts \
  -o <board>/fab/gerber/ <board>/<name>.kicad_pcb
kicad-cli pcb export drill -o <board>/fab/gerber/ <board>/<name>.kicad_pcb
```

then zipped from `fab/gerber/`. The panel yields no paste layers — it carries no
SMD pads.

## Regenerating

The panel comes from `eurorack/KoloredVerb.erbui`; its labels are baked vector
outlines and cannot be edited in KiCad:

```sh
python3 hardware/panel-board/regen_panel.py
```

The control board cannot be regenerated — `erbb build hardware` parses it with a
KiCad-6-era reader that throws on KiCad 10 net syntax. Edit it in KiCad and treat
the committed board as the source of truth.

The core board's assembly files are derived from its own PCB:

```sh
"…/KiCad/10.0/bin/python.exe" hardware/core-board/derive_assembly.py
```

## Licensing

The control board and panel are generated from eurorack-blocks blocks, and the
core board derives from its kivu12. All carry **CC BY-SA 4.0** by derivation.
See [THIRD-PARTY.md](../THIRD-PARTY.md).
