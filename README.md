# GlacialReverb

A 12 HP Eurorack stereo reverb built on a Daisy Patch SM mounted on a kivu12
carrier board, using the [eurorack-blocks](https://github.com/ohmtech-rdi/eurorack-blocks)
framework.

Eleven reverb voices grouped into six families, ten post-reverb tail effects
grouped into five families, and a freeze pad with ducking.

## Rooms

Selected with the **ROOM** button. Short press cycles the family (RGB colour),
long press cycles the variant within it (RGB brightness dips = variant number).

| Family  | Colour  | Variants                          |
|---------|---------|-----------------------------------|
| Hall    | blue    | FDN tank                          |
| Plate   | green   | Dattorro · modal · dispersive     |
| Spring  | cyan    | classic · modal · dispersive      |
| Shimmer | yellow  | octave up · octave down (Abyss)   |
| Cloud   | magenta | granular over an FDN tank         |
| Ambient | red     | modulated Hadamard FDN            |

The modal springs/plates are resonator banks at the physical eigenmodes; the
dispersive ones use stretched-allpass chains for the characteristic chirp.

## Tail effects

Selected with the **FX** button, same gesture (short = family, long = variant).
The red LED blinks the family number; None leaves the LED off. Each effect is
added over the untouched tail, so amount 0 is a true bypass.

| Family   | Variants                       |
|----------|--------------------------------|
| None     | —                              |
| Pitch    | octave up · sub · chord (5th)  |
| Modulate | chorus · formant               |
| Texture  | granular · reverse             |
| Resonant | resonator · droplet            |

## Panel

| Control   | Function                                                        |
|-----------|-----------------------------------------------------------------|
| FX pot    | Morph: model character and tail-effect depth together           |
| DECAY     | Tail length                                                     |
| MIX       | Wet added over the dry (additive, not a crossfade)              |
| PRE DL    | Pre-delay                                                       |
| DUCK      | Input ducks the reverb                                          |
| FRZ LV    | Frozen pad level                                                |
| FRZ MX    | How much new signal enters the frozen pad                       |
| FRZ btn   | Short press = freeze on/off · long press = tank-only on/off     |
| ROOM btn  | Short = room family · long = variant                            |
| FX btn    | Short = effect family · long = variant                          |
| FRZ G     | Gate: forces freeze on                                          |
| ROOM      | Trigger: advances the room family                               |
| FX / FRZ LV / MIX | CV over the corresponding pot                           |
| IN L/R    | Audio in (R normalled to L)                                     |
| OUT L/R   | Audio out                                                       |

CV drives continuous character; the buttons handle discrete structure. Room and
effect selection step discretely rather than sweeping: a switch renders both
voices through a 150 ms crossfade and clears the outgoing one only once it is
silent, so the tail carries across and the ROOM trigger can be clocked.

## Layout

```
eurorack/
  GlacialReverb.erbui        panel and control definition
  GlacialReverb.{h,cpp}      UI glue: selectors, gestures, LEDs
  GlacialReverbDsp.{h,cpp}   freeze pad, duck, mix, pre-delay wrapper
  dsp/                       reverb models, tail effects, shared DSP
  artifacts/hardware/        gerbers, BOM, panel, front PCB
```

## Building

See [BUILD.md](BUILD.md) for the toolchain, firmware, simulator and board
fabrication. The `eurorack-blocks` framework is not vendored in this repository.
