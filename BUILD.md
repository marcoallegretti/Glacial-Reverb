# Building and fabricating GlacialReverb

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
- KiCad for viewing the boards. The framework's panel generators shell out to a
  bundled KiCad 6 toolchain.

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

The plugin installs to `%LOCALAPPDATA%/Rack2/plugins-win-x64/GlacialReverb/`.
This target never touches the hardware files.

## Hardware fabrication

Deliverables live in `eurorack/artifacts/hardware/`:

| File                          | Use                                  |
|-------------------------------|--------------------------------------|
| `GlacialReverb.gerber.zip`    | Front PCB — send to the fab          |
| `GlacialReverb.panel.gerber.zip` | Panel — send to the fab           |
| `GlacialReverb.bom.csv`       | Bill of materials                    |
| `GlacialReverb.kicad_pcb`     | Front PCB source                     |
| `GlacialReverb.panel.*`       | Panel source, DXF/PDF drawings       |

Board is 12 HP. Send both gerber zips plus the BOM; the panel is a PCB, so it is
fabricated the same way as the board.

### Regenerating the panel

Panel artwork is generated from `GlacialReverb.erbui`; labels are baked to vector
outlines, so a label change requires regeneration rather than editing the board:

```python
import sys; sys.path.insert(0, '<eurorack-blocks>/build-system')
import erbui
ast = erbui.parse('eurorack/GlacialReverb.erbui')
erbui.generate_front_panel_pcb('eurorack/artifacts/hardware', ast)
```

### Front PCB: do not regenerate

`erbb build hardware` cannot regenerate `GlacialReverb.kicad_pcb`. The framework
reads the existing board with a KiCad 6 era parser, which throws on KiCad 10 net
syntax. Placement is owned by the `.erbui`, but apply front-PCB changes directly
in KiCad and keep the committed board as the source of truth. Only the panel
generators above are safe to re-run; hash the front PCB before and after any
build to confirm it is untouched.

## Verifying the DSP

The DSP is host-independent and can be exercised without hardware or Rack:
compile `GlacialReverbDsp.cpp` against a heap-backed `erb::SdramPtr` stub and
drive it with impulses and noise, measuring RT60, L/R correlation, peak, NaN and
stability across every model x effect combination.

Build with `-D_GLIBCXX_ASSERTIONS` so `std::array::operator[]` traps
out-of-range indices, and use the simulator's real sample rate (48014, not
48000). Delay-line index bugs can need seconds of runtime to surface, so soak
each combination rather than sampling it briefly.
