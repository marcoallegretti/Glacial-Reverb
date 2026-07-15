/*****************************************************************************

     GlacialReverb.cpp
     UI glue: two-tier ROOM/FX selectors, freeze gesture, LED rendering.

*Tab=3***********************************************************************/

/*\\ INCLUDE FILES \\\\*/

#include "GlacialReverb.h"

#include <cmath>
#include <cstddef>


namespace {

// family -> model id
constexpr int ROOM_MODEL [6][3] = {
   { 0,  0,  0 },   // Hall
   { 1,  9, 10 },   // Plate   : Dattorro, Modal, Dispersive
   { 5,  7,  8 },   // Spring  : classic,  Modal, Dispersive
   { 2,  6,  2 },   // Shimmer : up, Abyss (down)
   { 3,  3,  3 },   // Cloud
   { 4,  4,  4 },   // Ambient
};
constexpr int ROOM_VCOUNT [6] = { 1, 3, 3, 2, 1, 1 };

// family -> tail id
constexpr int FX_TAIL [5][3] = {
   { 0, 0, 0 },     // None
   { 2, 6, 8 },     // Pitch    : up, sub (down), chord
   { 3, 9, 3 },     // Modulate : chorus, formant
   { 4, 7, 4 },     // Texture  : granular, reverse
   { 1, 5, 1 },     // Resonant : resonator, droplet
};
constexpr int FX_VCOUNT [5] = { 1, 3, 2, 2, 2 };

inline float blink_count (float t_s, int n)
{
   if (n <= 0) return 0.f;
   float period = float (n) * 0.24f + 0.9f;
   float t = std::fmod (t_s, period);
   if (t >= float (n) * 0.24f) return 0.f;
   return (std::fmod (t, 0.24f) < 0.12f) ? 1.f : 0.f;
}

}  // namespace


/*\\ PUBLIC \\\\*/

void  GlacialReverb::process ()
{
   const float block_s = float (ui.audio_out_left.size ()) / float (erb_SAMPLE_RATE);
   constexpr float long_s = 0.4f;

   // one pot morphs model macro + tail depth together
   float fx = float (ui.fx_pot) + 0.5f * float (ui.fx_cv);
   if (fx > 1.f) fx = 1.f;
   if (fx < 0.f) fx = 0.f;
   dsp.set_fx (fx);
   dsp.set_tail_amount (fx);

   float duck = float (ui.duck_pot);
   if (duck > 1.f) duck = 1.f;
   if (duck < 0.f) duck = 0.f;
   dsp.set_duck (duck);

   float mix = float (ui.mix_pot) + 0.5f * float (ui.mix_cv);
   if (mix > 1.f) mix = 1.f;
   if (mix < 0.f) mix = 0.f;
   dsp.set_mix (mix);

   float decay = float (ui.decay_pot);
   if (decay > 1.f) decay = 1.f;
   if (decay < 0.f) decay = 0.f;
   dsp.set_decay (decay);

   float pre_delay = float (ui.pre_delay_pot);
   if (pre_delay > 1.f) pre_delay = 1.f;
   if (pre_delay < 0.f) pre_delay = 0.f;
   dsp.set_pre_delay (pre_delay);

   float frozen_level = float (ui.frozen_level_pot) + 0.5f * float (ui.frozen_level_cv);
   if (frozen_level > 1.f) frozen_level = 1.f;
   if (frozen_level < 0.f) frozen_level = 0.f;
   dsp.set_frozen_level (frozen_level);

   float frozen_mix = float (ui.frozen_mix_pot);
   if (frozen_mix > 1.f) frozen_mix = 1.f;
   if (frozen_mix < 0.f) frozen_mix = 0.f;
   dsp.set_frozen_mix (frozen_mix);

   // Freeze gesture: short = Freeze on/off, long = Tank-only on/off.
   if (ui.freeze_button.pressed ()) { freeze_hold_s = 0.f; freeze_long_fired = false; }
   if (ui.freeze_button.held ())
   {
      freeze_hold_s += block_s;
      if (! freeze_long_fired && freeze_hold_s >= long_s) { tank_state = ! tank_state; freeze_long_fired = true; }
   }
   if (ui.freeze_button.released () && ! freeze_long_fired) freeze_state = ! freeze_state;

   bool freeze_effective = freeze_state || bool (ui.freeze_gate);
   dsp.set_freeze (freeze_effective);

   // ROOM selector (fx_button): short = next family, long = next variant.
   if (ui.fx_button.pressed ()) { room_hold_s = 0.f; room_long_fired = false; }
   if (ui.fx_button.held ())
   {
      room_hold_s += block_s;
      if (! room_long_fired && room_hold_s >= long_s)
      {
         room_variant = (room_variant + 1) % ROOM_VCOUNT [room_family];
         dsp.set_fx_type (ROOM_MODEL [room_family][room_variant]);
         room_long_fired = true;
      }
   }
   if (ui.fx_button.released () && ! room_long_fired)
   {
      room_family = (room_family + 1) % 6;
      room_variant = 0;
      dsp.set_fx_type (ROOM_MODEL [room_family][room_variant]);
   }

   // ROOM-advance trigger (repurposed tank_gate): rising edge = next family
   {
      bool rg = bool (ui.tank_gate);
      if (rg && ! room_gate_prev)
      {
         room_family = (room_family + 1) % 6;
         room_variant = 0;
         dsp.set_fx_type (ROOM_MODEL [room_family][room_variant]);
      }
      room_gate_prev = rg;
   }

   // TailFX selector (tank_button): short = next family, long = next variant.
   if (ui.tank_button.pressed ()) { fx_hold_s = 0.f; fx_long_fired = false; }
   if (ui.tank_button.held ())
   {
      fx_hold_s += block_s;
      if (! fx_long_fired && fx_hold_s >= long_s)
      {
         fx_variant = (fx_variant + 1) % FX_VCOUNT [fx_family];
         dsp.set_tail_fx (FX_TAIL [fx_family][fx_variant]);
         fx_long_fired = true;
      }
   }
   if (ui.tank_button.released () && ! fx_long_fired)
   {
      fx_family = (fx_family + 1) % 5;
      fx_variant = 0;
      dsp.set_tail_fx (FX_TAIL [fx_family][fx_variant]);
   }

   bool tank_effective = tank_state;   // tank-only via freeze long-press; gate now drives ROOM
   dsp.set_tank (tank_effective);

   led_s += block_s;

   if (freeze_effective)
   {
      if (tank_effective) ui.freeze_led = 1.f;
      else
      {
         float ph  = std::fmod (led_s, 1.2f) / 1.2f;
         float tri = ph < 0.5f ? ph * 2.f : (1.f - ph) * 2.f;
         ui.freeze_led = 0.15f + 0.85f * tri;
      }
   }
   else ui.freeze_led = 0.f;

   {
      using erb::ColorRgb;
      if (! fx_led_init) { ui.fx_led.set_brightness (0.7f); fx_led_init = true; }

      ColorRgb col = ColorRgb {0.f, 0.f, 0.f};
      switch (room_family)
      {
      case 0: col = ColorRgb {0.f, 0.f, 1.f};  break; // Hall    blue
      case 1: col = ColorRgb {0.f, 1.f, 0.f};  break; // Plate   green
      case 2: col = ColorRgb {0.f, 1.f, 1.f};  break; // Spring  cyan
      case 3: col = ColorRgb {1.f, 0.85f,0.f}; break; // Shimmer yellow
      case 4: col = ColorRgb {1.f, 0.f, 1.f};  break; // Cloud   magenta
      case 5: col = ColorRgb {1.f, 0.f, 0.f};  break; // Ambient red
      default: break;
      }

      float b = 1.f;
      if (room_variant > 0)
      {
         float t = std::fmod (led_s, 1.8f);
         if (t < float (room_variant) * 0.34f)
            b = (std::fmod (t, 0.34f) < 0.17f) ? 0.2f : 1.f;
      }
      ui.fx_led.on (ColorRgb {col.r * b, col.g * b, col.b * b});
   }

   ui.tank_led = blink_count (led_s, fx_family);

   const float * const in [] = {
      &ui.audio_in_left [0],
      &ui.audio_in_right [0],
   };

   float * const out [] = {
      &ui.audio_out_left [0],
      &ui.audio_out_right [0],
   };

   dsp.process (out, in, ui.audio_out_left.size ());
}
