# kivu12 — JLCPCB ordering files

The kivu12 is the carrier board. It is **not** part of this repo's design — it
comes from the eurorack-blocks framework. These two files are *derived* from the
framework's own fab set; they are not a new design and not hand-authored.

## What these are

The framework ships `deps/eurorack-blocks/boards/kivu12/fab/`:

| file | |
|---|---|
| `kivu12.gerber.zip` | the board itself — **upload this one straight from `deps/`** |
| `kivu12-jlcpcb-bom.csv` | SMD parts **and** a `DNP` hand-solder section, split by `#` comment lines |
| `kivu12-cpl.csv` | 136 placements: the 97 SMD, plus 35 DNP parts and 4 tooling/logo rows |

JLCPCB's uploader stumbles on all three of those: the `#` lines can be read as
data, the 35 empty-LCSC rows each have to be dismissed by hand, and the tooling
rows match nothing. The files here are the same data with those rows removed:

| file | |
|---|---|
| `kivu12-jlcpcb-bom.csv` | 16 lines, **97 SMD parts**, every one with an LCSC number |
| `kivu12-jlcpcb-cpl.csv` | **97 placements**, all `top` |

Column layout, encoding (UTF-8 BOM) and line endings (CRLF) are byte-for-byte the
framework's. Only whole rows were dropped — no value was edited, and no new
format invented. Anything JLCPCB already accepted, it still accepts.

## Order

1. JLCPCB → *Add gerber file* → `deps/eurorack-blocks/boards/kivu12/fab/kivu12.gerber.zip`
2. Turn on **PCB Assembly**
3. BOM → `kivu12-jlcpcb-bom.csv` (this directory)
4. CPL → `kivu12-jlcpcb-cpl.csv` (this directory)

It arrives with the 97 SMD parts placed. **You still hand-solder 22 through-hole
parts** — 1 IDC power header, 11 pin sockets, 8 jumper headers, the microSD
socket and the Daisy Patch SM socket. No fine-pitch work. The Daisy Patch SM
module is a separate purchase.

## How these were checked

Generated and verified against `deps/eurorack-blocks/boards/kivu12/kivu12.kicad_pcb`
(136 footprints, matching the 136 CPL rows). Note that is the board in the kivu12
**root** — `hardware/kivu12.kicad_pcb` is a stale 80-footprint KiCad 6 file with no
resistors on it, and is the wrong reference.

Checks, all passing:

- no duplicate designators (97)
- every line carries a valid LCSC number
- no designator appears in both the SMD and DNP sections
- every SMD part has exactly one placement, and every placement has a BOM line
- every part exists on the PCB, and every part JLCPCB will place is genuinely SMD
- **placements agree with the PCB under a zero offset** — `CPL.X == PCB.x` and
  `CPL.Y == -PCB.y` for all 97, which is what proves the coordinates are right
  rather than merely plausible
- no `#` comment lines; no hand-solder or tooling reference leaked in

Regenerate with `metamodule/../tools`-style scripting only if the framework's fab
set changes; otherwise treat these as derived artefacts of that set.
