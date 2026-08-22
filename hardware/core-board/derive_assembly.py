#!/usr/bin/env python3
"""Derive the core board's JLCPCB assembly BOM and CPL from the kivu12 set.

Run with KiCad 10's bundled Python.
"""

import csv
import io
import os
import sys

import pcbnew

CORE = os.path.dirname(os.path.abspath(__file__))
REF = CORE + "/reference-kivu12"
BOARD = CORE + "/KoloredVerb.core.kicad_pcb"


def read(path):
    with io.open(path, encoding="utf-8-sig", newline="") as f:
        return list(csv.reader(f))


def write(path, rows):
    with io.open(path, "w", encoding="utf-8-sig", newline="") as f:
        csv.writer(f, lineterminator="\r\n", quoting=csv.QUOTE_MINIMAL).writerows(rows)


board = pcbnew.LoadBoard(BOARD)
smd = {fp.GetReference(): fp for fp in board.GetFootprints()
       if fp.GetAttributes() & pcbnew.FP_SMD}
print("SMD footprints on the board: %d" % len(smd))

# ---- BOM ------------------------------------------------------------------
bom = read(REF + "/kivu12-jlcpcb-bom.csv")
out = [bom[0]]
kept_bom = set()
for row in bom[1:]:
    if not row or not row[0].strip():
        continue
    refs = [r.strip() for r in row[1].split(",") if r.strip()]
    alive = [r for r in refs if r in smd]
    if not alive:
        print("  line dropped entirely: %s (%d parts)" % (row[0], len(refs)))
        continue
    if len(alive) != len(refs):
        print("  %-14s %d -> %d" % (row[0], len(refs), len(alive)))
    kept_bom.update(alive)
    out.append([row[0], ",".join(alive), row[2], row[3]])
write(CORE + "/fab/KoloredVerb.core-jlcpcb-bom.csv", out)
print("BOM: %d lines, %d parts" % (len(out) - 1, len(kept_bom)))

# ---- CPL ------------------------------------------------------------------
cpl = read(REF + "/kivu12-jlcpcb-cpl.csv")
out = [cpl[0]]
kept_cpl = set()
drift = []
for row in cpl[1:]:
    if not row:
        continue
    ref = row[0].strip('"')
    if ref not in smd:
        continue
    kept_cpl.add(ref)
    pos = smd[ref].GetPosition()
    x, y = pcbnew.ToMM(pos.x), pcbnew.ToMM(pos.y)
    if abs(x - float(row[3])) > 1e-4 or abs(-y - float(row[4])) > 1e-4:
        drift.append((ref, row[3], row[4], round(x, 6), round(-y, 6)))
    out.append(row)
write(CORE + "/fab/KoloredVerb.core-jlcpcb-cpl.csv", out)
print("CPL: %d placements" % (len(out) - 1))

# ---- checks ---------------------------------------------------------------
fail = 0
if kept_bom != set(smd):
    fail += 1
    print("*** BOM/board mismatch: only-BOM=%s only-board=%s"
          % (sorted(kept_bom - set(smd)), sorted(set(smd) - kept_bom)))
if kept_cpl != set(smd):
    fail += 1
    print("*** CPL/board mismatch: only-CPL=%s only-board=%s"
          % (sorted(kept_cpl - set(smd)), sorted(set(smd) - kept_cpl)))
if drift:
    fail += 1
    print("*** %d placements disagree with the board:" % len(drift))
    for d in drift[:10]:
        print("      %s ref(%s,%s) board(%s,%s)" % d)
if not fail:
    print("\nchecked: BOM designators == CPL designators == the %d SMD parts on the"
          " board, and CPL.X == PCB.x / CPL.Y == -PCB.y for every one" % len(smd))
else:
    sys.exit("assembly files failed their checks")
