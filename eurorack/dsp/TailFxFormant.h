/*****************************************************************************

     TailFxFormant.h
     Formant: LFO-swept vowel bandpasses.

*Tab=3***********************************************************************/

#pragma once

#include "TailFx.h"

#include <cmath>

namespace dsp
{

class TailFxFormant : public TailFx
{
public:
   explicit       TailFxFormant (float sample_freq);

   void           set_amount (float a) override
   {
      _amount = clamp01 (a);
      _q = 0.30f - _amount * 0.17f;
   }

   void           reset () override
   {
      _l1l = _b1l = _l2l = _b2l = 0.f;
      _l1r = _b1r = _l2r = _b2r = 0.f;
      _lfo = 0.f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      float f1l, f2l, f1r, f2r;
      vowel (_lfo, f1l, f2l);
      vowel (_lfo + 2.1f, f1r, f2r);
      _lfo += _lfo_inc; if (_lfo >= 6.2831853f) _lfo -= 6.2831853f;

      float wl = svf (_l1l, _b1l, in.left,  coef (f1l)) * 0.9f
               + svf (_l2l, _b2l, in.left,  coef (f2l)) * 0.7f;
      float wr = svf (_l1r, _b1r, in.right, coef (f1r)) * 0.9f
               + svf (_l2r, _b2r, in.right, coef (f2r)) * 0.7f;

      // the two resonators are near self-oscillation at low q, so 1.4 put this
      // +10 dB above every other tail and slammed the wrapper limiter
      float g = _amount * 0.35f;
      return { in.left + g * wl, in.right + g * wr };
   }

private:
   static constexpr int NV = 5;

   float          coef (float f) const { return 2.f * std::sin (3.1415927f * f / _sr); }

   float          svf (float & low, float & band, float in, float f) const
   {
      low  += f * band;
      float high = in - low - _q * band;
      band += f * high;
      return band;
   }

   void           vowel (float phase, float & f1, float & f2) const
   {
      float u = (0.5f - 0.5f * std::cos (phase)) * float (NV);
      int i = int (u); if (i >= NV) i = NV - 1;
      int j = i + 1; if (j >= NV) j = 0;
      float t = u - float (i);
      f1 = _F1[i] + (_F1[j] - _F1[i]) * t;
      f2 = _F2[i] + (_F2[j] - _F2[i]) * t;
   }

   float          _sr;
   float          _amount = 0.f, _q = 0.25f;
   float          _l1l = 0.f, _b1l = 0.f, _l2l = 0.f, _b2l = 0.f;
   float          _l1r = 0.f, _b1r = 0.f, _l2r = 0.f, _b2r = 0.f;
   float          _lfo = 0.f, _lfo_inc = 0.f;

   static constexpr float _F1 [NV] = { 730.f, 530.f, 270.f, 570.f, 300.f };   // a e i o u
   static constexpr float _F2 [NV] = { 1090.f, 1840.f, 2290.f, 840.f, 870.f };
};

inline TailFxFormant::TailFxFormant (float sample_freq)
:  _sr (sample_freq)
{
   _lfo_inc = 6.2831853f * 0.28f / _sr;
   reset ();
}

}  // namespace dsp
