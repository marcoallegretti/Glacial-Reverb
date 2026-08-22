# Third-party code and licences

KoloredVerb is not standalone work. What follows is what it stands on, who owns
it, and what that obliges us to do. Each entry was traced to a file in this
repository or in `deps/`, not assumed.

## Vendored into this repository

### `eurorack/dsp/ReverbSc.{h,cpp}` — MIT, © 2017 Raphael Dinge

Copied from `eurorack-blocks`, `samples/dsp/ReverbSc.{h,cpp}`. Verified identical
by its delay table (`{ 2473, 44, 14226, 1966 }`, upstream line 190).

This file is load-bearing: Hall wraps it, Shimmer and Abyss wrap it, Cloud runs
granular over it, and the freeze pad *is* it. Twelve of twelve rooms depend on it.

It is vendored rather than referenced so the DSP compiles standalone — against a
heap-backed `erb::SdramPtr` stub for the offline harness, and directly into the
MetaModule plugin, which has no eurorack-blocks at all.

The algorithm is Sean Costello's `reverbsc` (Stautner–Puckette FDN) as it appears
in Csound; the implementation here is Raphael Dinge's.

> **The copyright line above was missing.** Both files carried only
> `ReverbSc.cpp (local copy)` until it was restored. MIT's single obligation is
> that the notice travels with the copy, so that omission was a licence
> violation for as long as it stood. It is recorded here rather than quietly
> fixed.

## Built against, not vendored

### eurorack-blocks — MIT (software) / CC BY-SA 4.0 (hardware)

Per its README: *"All files in this repository, excluding `submodules/`, are
provided with the CC BY-SA 4.0 license for the hardware part, and MIT license for
the software part"*, with exceptions listed there (fonts under SIL OFL, etc.).

Both halves reach this project, and they are not the same licence:

- **Software (MIT)** — `erb::SdramPtr`, the `.erbui` code generators, the block
  library, the VCV Rack and Daisy build targets.
- **Hardware (CC BY-SA 4.0)** — the control board and panel are generated from
  eurorack-blocks blocks and the kivu12 board. **ShareAlike is copyleft**: the
  boards in `hardware/` carry BY-SA obligations regardless of
  the licence chosen for the software.

### kivu12 carrier board — part of eurorack-blocks (CC BY-SA 4.0, hardware)

**`hardware/core-board/` is an adaptation of the eurorack-blocks kivu12 rev7**,
by Raphael Dinge, used under CC BY-SA 4.0 and shared under the same licence.

Modified: parts this module does not use were removed (the second CD4051 and
CD4021 with their front ends, the CV-pitch conditioning block, the microSD block
and eleven LED resistors), CI1/CI2 were hard-wired to the ADC in place of the
jumpers, two freed Daisy inputs were tied to GND, and the eurorack-blocks logo
was removed from the silkscreen. The remaining routing is the original author's.

`hardware/core-board/reference-kivu12/` holds JLCPCB ordering files *derived*
from the framework's own fab set — rows removed, no values edited.

### Daisy Patch SM / libDaisy — Electrosmith

Reached as an eurorack-blocks submodule. `submodules/` are excluded from the
eurorack-blocks licence and carry their own.

## Status

- [x] ReverbSc copyright restored (`.cpp` and `.h`)
- [x] `LICENSE` — EUPL 1.2 for this project's own work
- [ ] MetaModule SDK terms — tracked on the `metamodule-port` branch, which is
      not part of this (master) history and must be settled before that branch
      is published. `metamodule/plugin.json` there still says
      `"license": "proprietary"`, a placeholder.

## On choosing a licence for our own work

MIT and CC BY-SA 4.0 upstream leave the software side open: MIT is permissive, so
MIT code may be carried into a copyleft work provided its notice travels with it —
which is what the restored header and this file are for.

The hardware is the constraint, not the software. CC BY-SA 4.0 attaches to the
boards by derivation, and a software licence does not displace it. Whatever is
chosen for the code, the boards stay ShareAlike.

This is a record of what the files say, not legal advice.
