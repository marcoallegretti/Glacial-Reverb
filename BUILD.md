# Building and fabricating Koloured-Verb

## Toolchain

- [eurorack-blocks](https://github.com/ohmtech-rdi/eurorack-blocks) — not vendored
  here. Clone it and initialise its submodules (`gyp-next` is required to generate
  the build; `vcv-rack-sdk` for the simulator):

  ```sh
  git submodule update --init submodules/gyp-next submodules/vcv-rack-sdk
  ```

  Point the project at it via `dep/eurorack-blocks` (a link or junction).
- Python 3.12 (the framework's bundled `python3-packages` are built for cp312).
- MinGW-w64 g++ (MSYS2 `mingw64`) on Windows.
- KiCad 10 for the boards and `kicad-cli` gerber export. The framework's panel
  generators separately shell out to a **bundled KiCad 6**, which lives under
  `build-system/toolchain/` and is not part of the source tree.

### Local layout on this machine

The framework is kept in `deps/eurorack-blocks/` so builds do not reach into
whichever scattered clone happens to be on `PATH`. Three links make that work —
all inside gitignored `dep/`/`deps/`, so none of this is committed:

| Link | Target | Why |
|---|---|---|
| `dep/eurorack-blocks` | `deps/eurorack-blocks` | what the build system looks for |
| `deps/eurorack-blocks/submodules/gyp-next` | a populated clone | needed to *generate* the build |
| `deps/eurorack-blocks/submodules/vcv-rack-sdk` | a populated SDK | `libRack` for the simulator |
| `deps/eurorack-blocks/build-system/toolchain` | the bundled toolchain | KiCad 6 + cairo DLLs for the panel generators |

Invoke the **vendored** `erbb`, not whichever one is on `PATH`:

```sh
deps/eurorack-blocks/build-system/scripts/erbb
```

This matters: the generated `KoloredVerbUi.h` bakes in the path to
`BoardKivu12.h`, so an `erbb` from a different clone silently compiles the
firmware against that clone's framework instead of the vendored one.

## Firmware (Daisy)

```sh
erbb configure
erbb build daisy --configuration release
```

## Simulator (VCV Rack)

```sh
erbb configure
erbb build simulator --configuration debug
```

Headless on Windows, MSYS `make` drops the MinGW `PATH` when spawning `g++`, so
the compile step fails even though the source generation succeeded. Run make
directly from a shell that has the environment set:

```sh
export PATH="/c/msys64/mingw64/bin:$PATH"
export HOME=/c/Users/<you>; export MSYSTEM=MINGW64
cd eurorack/artifacts/simulator && make -j1 install
```

The plugin installs to `%LOCALAPPDATA%/Rack2/plugins-win-x64/KoloredVerb/`.
This target never touches the hardware files.

### The generated manifest carries the wrong identity

`erbui/generators/vcvrack/manifest.py` writes `plugin.json` with the framework's
identity hardcoded — slug `ErbPluginKoloredVerb`, brand and author `Erb`, every
URL pointing at ohmtech-rdi, and `"license": "proprietary"` — and it rewrites the
file on **every** build, so the correction cannot be committed into the artifact.
Stamp it afterwards:

```sh
python3 ci/vcv_manifest.py $(find eurorack/artifacts -name plugin.json)
```

That sets the 23DSP identity and the EUPL-1.2 licence, and leaves the module slug
alone — it has to keep matching the string `plugin_vcvrack.cpp` passes to
`rack::createModel`. The release workflow runs this step automatically and also
stamps the tag as the version.

## Hardware fabrication

Deliverables live under `hardware/`, one folder per board:

| Board | Gerbers | BOM |
|---|---|---|
| Core | `core-board/fab/KoloredVerb.core.gerber.zip` | `core-board/KoloredVerb.core.bom.csv` |
| Control | `control-board/fab/KoloredVerb.gerber.zip` | `control-board/KoloredVerb.bom.csv` |
| Panel | `panel-board/fab/KoloredVerb.panel.gerber.zip` | none — mechanical |

All 2-layer, 1.6 mm FR4; the panel is fabricated as a PCB like the boards.

The core board's 45 SMD parts are assembled by JLCPCB from
`core-board/fab/KoloredVerb.core-jlcpcb-bom.csv` and `-cpl.csv`; its 12
through-hole parts and the whole control board are hand-soldered. `ZZ10` on the
control board is DNP — the core board has no microSD, so it has no mating socket.

### Regenerating the panel

The panel is FR4 (`material pcb` in the `.erbui`), so it is fabricated from
gerbers like the other two boards — no DXF or PDF is emitted, and none is
needed. Labels are baked to vector outlines and cannot be edited in KiCad, so
any label or control change has to come back through the `.erbui`:

```sh
python3 hardware/panel-board/regen_panel.py
```

Use a Python **3.12** interpreter — the framework's bundled `python3-packages`
(cffi, cairo) are cp312 builds.

The script calls only the four `generate_front_panel_*` steps, then re-exports
the gerbers with `kicad-cli`. It deliberately does **not** call
`erbb build hardware`, which would run `generate_front_pcb_kicad_pcb` against
the frozen control board. As a backstop it sha256s the control board before and
after and fails loudly if it moved.

Regeneration is faithful but not byte-stable: KiCad footprint UUIDs and the
footprint ordering are regenerated each run, so the `.kicad_pcb` churns while
the board does not. Verify by exported geometry, not by file hash — after the
rename regeneration the vertex set of every gerber layer was unchanged.

### Control board: do not regenerate

`erbb build hardware` cannot regenerate `KoloredVerb.kicad_pcb`. The framework
reads the existing board with a KiCad 6 era parser, which throws on KiCad 10 net
syntax. Placement is owned by the `.erbui`, but apply control-board changes directly
in KiCad and keep the committed board as the source of truth. Only the panel
generators above are safe to re-run; hash the control board before and after any
build to confirm it is untouched.

## Verifying the DSP

The DSP is host-independent and can be exercised without hardware or Rack:
compile `KoloredVerbDsp.cpp` against a heap-backed `erb::SdramPtr` stub and
drive it with impulses and noise, measuring RT60, L/R correlation, peak, NaN and
stability across every model x effect combination.

Build with `-D_GLIBCXX_ASSERTIONS` so `std::array::operator[]` traps
out-of-range indices, and use the simulator's real sample rate (48014, not
48000). Delay-line index bugs can need seconds of runtime to surface, so soak
each combination rather than sampling it briefly.
