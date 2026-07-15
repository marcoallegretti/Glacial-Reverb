/*****************************************************************************

     ReverbPlateVintage.h
     Plate (vintage): steel-plate voicing, HF decay clamped off the decay knob.

*Tab=3***********************************************************************/

#pragma once

#include "ReverbModel.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class ReverbPlateVintage : public ReverbModel
{
public:
   explicit       ReverbPlateVintage (float sample_freq);

   // measured plates run ~1 s (damper closed) to ~5 s (open) at 500 Hz
   void           set_decay (float d) override
   {
      _gain = 0.820f + clamp01 (d) * 0.141f;
      update_damp ();
   }

   // macro = damper travel: the physical plate damper trades decay for a
   // response that flattens out with frequency as it closes in
   void           set_macro (float fx) override { _tone = clamp01 (fx); update_damp (); }

   void           set_low_pass_freq (float) override {}   // voicing is fixed: it is the instrument

   void           reset () override
   {
      auto & B = *_buf;
      B.d0.fill (0.f); B.d1.fill (0.f); B.d2.fill (0.f); B.d3.fill (0.f);
      for (auto & a : B.di) a.fill (0.f);
      _wp = 0;
      _sh0 = _sh1 = _sh2 = _sh3 = 0.f;
      _in_lp = 0.f; _o_lp_l = _o_lp_r = _o_hp_l = _o_hp_r = 0.f;
      _lfo = 0.f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      auto & B = *_buf;

      // the exciter drives a steel sheet, not a room: nothing above ~3 kHz gets
      // into the plate in the first place
      _in_lp += _in_c * (0.5f * (in.left + in.right) - _in_lp);
      float x = diffuse (_in_lp);

      float md = 1.6f * std::sin (_lfo);
      _lfo += _lfo_inc; if (_lfo >= 6.2831853f) _lfo -= 6.2831853f;

      float a = read (B.d0, float (L0) + md);
      float b = read (B.d1, float (L1) - md);
      float c = read (B.d2, float (L2) + md * 0.7f);
      float d = read (B.d3, float (L3) - md * 0.7f);

      a = shelf (_sh0, a); b = shelf (_sh1, b);
      c = shelf (_sh2, c); d = shelf (_sh3, d);

      float s0 = 0.5f * ( a + b + c + d);
      float s1 = 0.5f * ( a - b + c - d);
      float s2 = 0.5f * ( a + b - c - d);
      float s3 = 0.5f * ( a - b - c + d);

      B.d0 [_wp & DMASK] = soft (x + _gain * s0);
      B.d1 [_wp & DMASK] = soft (x + _gain * s1);
      B.d2 [_wp & DMASK] = soft (x + _gain * s2);
      B.d3 [_wp & DMASK] = soft (x + _gain * s3);
      ++_wp;

      // pickups sat at unequal distances from the exciter, so L/R never matched
      float yl = 0.6f * a + 0.4f * c;
      float yr = 0.6f * d + 0.4f * b;
      return { band (_o_lp_l, _o_hp_l, yl) * NORM, band (_o_lp_r, _o_hp_r, yr) * NORM };
   }

private:
   static constexpr unsigned D = 2048, DMASK = 2047;
   static constexpr unsigned DI = 512, DIMASK = 511;
   // a 2 m plate crosses in ~20 ms at 500 Hz
   static constexpr int L0 = 953, L1 = 1193, L2 = 1511, L3 = 1889;
   // the late tail is set by the slowest line, not the average
   static constexpr float L_REF = float (L3);
   static constexpr float NORM = 1.35f;

   // a real plate holds ~1.3 modes/Hz against the ~3/Hz that reads as
   // colourless, so it is coloured by construction: diffuse less than the
   // bright plate does, or the voice stops being a plate
   static constexpr int DL [3] = { 113, 191, 269 };

   struct Buffers
   {
      std::array<float,D>                d0{}, d1{}, d2{}, d3{};
      std::array<std::array<float,DI>,3> di{};
   };

   static float   soft (float x)
   {
      if (x < -1.5f) return -1.f;
      if (x >  1.5f) return  1.f;
      return x - x * x * x * (1.f / 6.75f);
   }

   // Measured plates hold HF decay near 1-1.5 s at every damper setting while
   // the low end runs several times longer, so HF loss has to cancel the decay
   // gain instead of adding to it. Short settings flatten out because the
   // required relative gain clips at 1 -- which is what the plate does too.
   void           update_damp ()
   {
      float t60_hf = 1.05f + _tone * 0.55f;
      float g_hf = std::pow (10.f, -3.f * L_REF / (t60_hf * _sr));
      _hf_rel = g_hf / _gain;
      if (_hf_rel > 1.f) _hf_rel = 1.f;
      if (_hf_rel < 0.05f) _hf_rel = 0.05f;
   }

   float          shelf (float & s, float x) const
   {
      s += _sh_c * (x - s);                     // DC gain 1, HF gain _hf_rel
      return _hf_rel * x + (1.f - _hf_rel) * s;
   }

   // plate response peaks near 300 Hz and is ~18 dB down by 10 kHz
   float          band (float & lp, float & hp, float x) const
   {
      lp += _o_lp_c * (x - lp);
      hp += _o_hp_c * (lp - hp);
      return lp - hp;
   }

   float          diffuse (float x)
   {
      auto & B = *_buf;
      for (int i = 0 ; i < 3 ; ++i)
      {
         auto & buf = B.di[i];
         float dv = buf [(_wp - unsigned (DL[i])) & DIMASK];
         float y = -0.62f * x + dv;
         buf [_wp & DIMASK] = x + 0.62f * y;
         x = y;
      }
      return x;
   }

   float          read (const std::array<float,D> & b, float delay) const
   {
      float rp = float (_wp & DMASK) - delay;
      while (rp < 0.f) rp += float (D);
      int i1 = int (rp) & int (DMASK); float f = rp - std::floor (rp);
      int i0 = (i1 - 1) & int (DMASK), i2 = (i1 + 1) & int (DMASK), i3 = (i1 + 2) & int (DMASK);
      float y0 = b[i0], y1 = b[i1], y2 = b[i2], y3 = b[i3];
      float c1 = 0.5f*(y2-y0), c2 = y0 - 2.5f*y1 + 2.f*y2 - 0.5f*y3, c3 = 0.5f*(y3-y0)+1.5f*(y1-y2);
      return ((c3*f + c2)*f + c1)*f + y1;
   }

   static float   one_pole (float fc, float sr)
   {
      float c = 1.f - std::exp (-6.2831853f * fc / sr);
      if (c < 0.f) c = 0.f;
      if (c > 1.f) c = 1.f;
      return c;
   }

   float          _sr;
   erb::SdramPtr<Buffers> _buf;
   float          _gain = 0.85f, _tone = 0.5f, _hf_rel = 0.5f;
   float          _in_c = 0.2f, _sh_c = 0.3f, _o_lp_c = 0.2f, _o_hp_c = 0.02f;
   float          _sh0 = 0.f, _sh1 = 0.f, _sh2 = 0.f, _sh3 = 0.f;
   float          _in_lp = 0.f;
   float          _o_lp_l = 0.f, _o_lp_r = 0.f, _o_hp_l = 0.f, _o_hp_r = 0.f;
   float          _lfo = 0.f, _lfo_inc = 0.f;
   unsigned       _wp = 0;
};

inline ReverbPlateVintage::ReverbPlateVintage (float sample_freq)
:  _sr (sample_freq)
,  _buf (erb::make_sdram<Buffers> ())
{
   _in_c   = one_pole (2800.f, _sr);
   _sh_c   = one_pole (1000.f, _sr);   // low corner: the shelf must reach its
                                       // asymptote by 8 kHz or the clamp leaks
   _o_lp_c = one_pole (1800.f, _sr);
   _o_hp_c = one_pole ( 130.f, _sr);
   _lfo_inc = 6.2831853f * 1.0f / _sr;   // Dattorro's tank runs ~1 Hz
   update_damp ();
   reset ();
}

}  // namespace dsp
