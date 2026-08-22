#!/usr/bin/env python3
"""Regenerate the panel from KoloredVerb.erbui. Needs Python 3.12."""

import hashlib
import json
import io
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

ERBUI = os.path.join(ROOT, "eurorack", "KoloredVerb.erbui")
CONTROL_BOARD = os.path.join(ROOT, "hardware", "control-board", "KoloredVerb.kicad_pcb")
BOARD = os.path.join(HERE, "KoloredVerb.panel.kicad_pcb")
PROJECT = os.path.join(HERE, "KoloredVerb.panel.kicad_pro")
FAB = os.path.join(HERE, "fab")

GERBER_LAYERS = ("F.Cu,B.Cu,F.Mask,B.Mask,F.Paste,B.Paste,"
                 "F.Silkscreen,B.Silkscreen,Edge.Cuts")
MODULE = "KoloredVerb"
SOURCE_SUFFIXES = (".kicad_pcb", ".kicad_mod", ".kicad_pro", ".svg", ".dxf", ".pdf", ".txt")
EXTRA_SOURCES = ("fp-lib-table",)

MOUNTING_HOLE = """(module _MountingHole (layer F.Cu) (tedit 64651566)
  (attr through_hole)
  (fp_text reference "" (at 0 0) (layer F.SilkS)
    (effects (font (size 1.27 1.27) (thickness 0.15)))
  )
  (fp_text value "" (at 0 0) (layer F.SilkS)
    (effects (font (size 1.27 1.27) (thickness 0.15)))
  )
  (pad "" thru_hole oval locked (at 0 0) (size 7.4 4.2) (drill oval 6.4 3.2) (layers *.Cu *.Mask))
)
"""


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def framework():
    for candidate in ("dep/eurorack-blocks", "deps/eurorack-blocks"):
        path = os.path.join(ROOT, candidate, "build-system")
        if os.path.isdir(path):
            return path
    sys.exit("no eurorack-blocks at dep/ or deps/ -- see BUILD.md")


def kicad_cli():
    found = shutil.which("kicad-cli")
    if found:
        return found
    fallback = r"C:\Users\might\AppData\Local\Programs\KiCad\10.0\bin\kicad-cli.exe"
    if os.path.exists(fallback):
        return fallback
    sys.exit("kicad-cli not found")


def generate(out_dir):
    sys.path.insert(0, framework())
    os.environ.setdefault("HOME", os.path.expanduser("~"))
    os.environ.setdefault("USERPROFILE", os.path.expanduser("~"))
    os.environ.setdefault("XDG_CONFIG_HOME", os.path.join(os.path.expanduser("~"), ".config"))
    os.environ.setdefault("MSYSTEM", "MINGW64")

    import erbui

    ast = erbui.parse(ERBUI)
    for step in (erbui.generate_front_panel_dxf,
                 erbui.generate_front_panel_pdf,
                 erbui.generate_front_panel_pcb,
                 erbui.generate_front_panel_specs):
        print("  %s" % step.__name__)
        step(out_dir, ast)
    # generate_front_pcb_kicad_pcb would rewrite the frozen control board.


def export_gerbers():
    cli = kicad_cli()
    gerber_dir = os.path.join(FAB, "gerber")
    shutil.rmtree(gerber_dir, ignore_errors=True)
    os.makedirs(gerber_dir)
    subprocess.check_call([cli, "pcb", "export", "gerbers", "--no-protel-ext",
                           "--layers", GERBER_LAYERS, "-o", gerber_dir + os.sep, BOARD],
                          stdout=subprocess.DEVNULL)
    subprocess.check_call([cli, "pcb", "export", "drill", "-o", gerber_dir + os.sep, BOARD],
                          stdout=subprocess.DEVNULL)
    zip_path = os.path.join(FAB, "KoloredVerb.panel.gerber.zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for name in sorted(os.listdir(gerber_dir)):
            z.write(os.path.join(gerber_dir, name), name)
    return len(os.listdir(gerber_dir))


def restore_project_rules():
    # The generator writes a fresh .kicad_pro; the panel carries no nets, so
    # every copper island a cutout separates is isolated by construction.
    with io.open(PROJECT, encoding="utf-8") as f:
        data = json.load(f)
    rules = data.setdefault("board", {}).setdefault("design_settings", {}) \
                .setdefault("rule_severities", {})
    rules["isolated_copper"] = "ignore"
    # footprints are embedded in the board, so a CI container without the
    # Local library resolved still fabricates correctly
    rules["lib_footprint_issues"] = "ignore"
    rules["lib_footprint_mismatch"] = "ignore"
    with io.open(PROJECT, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=2)


def main():
    before = sha256(CONTROL_BOARD)
    print("control board sha256 before: %s" % before[:16])

    tmp = tempfile.mkdtemp(prefix="kv-panel-")
    try:
        print("generating panel into %s" % tmp)
        generate(tmp)

        installed = []
        for name in sorted(os.listdir(tmp)):
            if name in EXTRA_SOURCES or (name.startswith(MODULE)
                                         and name.endswith(SOURCE_SUFFIXES)):
                shutil.copy2(os.path.join(tmp, name), os.path.join(HERE, name))
                installed.append(name)
        print("installed %d: %s" % (len(installed), ", ".join(installed)))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    # not emitted by the generator, but the board references Local:_MountingHole
    with io.open(os.path.join(HERE, "_MountingHole.kicad_mod"), "w",
                 encoding="utf-8", newline="\n") as f:
        f.write(MOUNTING_HOLE)
    restore_project_rules()

    print("exported %d gerber files" % export_gerbers())

    after = sha256(CONTROL_BOARD)
    print("control board sha256 after:  %s" % after[:16])
    if before != after:
        sys.exit("*** the control board changed -- restore it from git ***")
    print("control board untouched")


if __name__ == "__main__":
    main()
