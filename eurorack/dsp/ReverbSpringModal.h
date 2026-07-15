/*****************************************************************************

     ReverbSpringModal.h
     Spring (modal): resonator bank at the spring eigenmodes.

*Tab=3***********************************************************************/

#pragma once

#include "ReverbModel.h"

#include <cmath>

namespace dsp
{

class ReverbSpringModal : public ReverbModel
{
public:
   explicit       ReverbSpringModal (float sample_freq);

   void           set_decay (float d) override
   {
      d = clamp01 (d);
      if (d == _decay) return;
      _decay = d;
      update_decay ();
   }

   void           set_macro (float fx) override
   {
      fx = clamp01 (fx);
      if (fx == _macro) return;
      _macro = fx;
      update_gains ();
   }

   void           set_low_pass_freq (float f) override
   {
      _out_c = 1.f - std::exp (-6.2831853f * f / _sr);
   }

   void           reset () override
   {
      for (int n = 0 ; n < N ; ++n) { _y1[n] = _y2[n] = 0.f; }
      _olp_l = _olp_r = 0.f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      float x = 0.5f * (in.left + in.right);
      float out_l = 0.f, out_r = 0.f;
      for (int n = 0 ; n < N ; ++n)
      {
         float y = _a1[n] * _y1[n] - _a2[n] * _y2[n] + x * _ig[n];   // resonator Q ~ 1/(1-r): normalise or it clips
         _y2[n] = _y1[n]; _y1[n] = y;
         out_l += y * _cl[n];
         out_r += y * _cr[n];
      }
      out_l *= NORM; out_r *= NORM;
      _olp_l += _out_c * (out_l - _olp_l);
      _olp_r += _out_c * (out_r - _olp_r);
      return { _olp_l, _olp_r };
   }

private:
   static constexpr int   N = 64;
   static constexpr float NORM = 0.5f;
   static constexpr float F_LO = 80.f, F_HI = 6000.f, F_REF = 500.f;

   static float   hashf (int n, float a, float b)
   {
      float s = std::sin (float (n) * a + b) * 43758.5453f;
      return s - std::floor (s);
   }

   void           update_decay ()
   {
      float t60_base = 0.4f + _decay * 3.2f;
      for (int n = 0 ; n < N ; ++n)
      {
         float t60 = t60_base * std::sqrt (F_REF / _f[n]);
         if (t60 < 0.05f) t60 = 0.05f;
         float r = std::exp (-6.9078f / (t60 * _sr));        // -60 dB over t60
         if (r > 0.99995f) r = 0.99995f;
         _a1[n] = _w[n] * r;
         _a2[n] = r * r;
         _ig[n] = 1.f - r;                                   // Q ~ 1/(1-r): normalise or it clips
      }
   }

   void           update_gains ()
   {
      for (int n = 0 ; n < N ; ++n)
      {
         float u = (_f[n] - F_LO) / (F_HI - F_LO);
         float tilt = 1.f - u * (1.f - _macro);
         float g = _base_gain[n] * tilt;
         _cl[n] = g * _pan_l[n];
         _cr[n] = g * _pan_r[n];
      }
   }

   float          _sr;
   float          _decay = -1.f, _macro = -1.f, _out_c = 0.5f;
   float          _w [N] {}, _f [N] {};
   float          _a1 [N] {}, _a2 [N] {}, _ig [N] {};
   float          _base_gain [N] {};
   float          _cl [N] {}, _cr [N] {};
   float          _pan_l [N] {}, _pan_r [N] {};
   float          _y1 [N] {}, _y2 [N] {};
   float          _olp_l = 0.f, _olp_r = 0.f;
};

inline ReverbSpringModal::ReverbSpringModal (float sample_freq)
:  _sr (sample_freq)
{
   for (int n = 0 ; n < N ; ++n)
   {
      float u = float (n) / float (N - 1);
      float f = F_LO * std::pow (F_HI / F_LO, u);
      f *= 1.f + 0.5f * u * u;
      f *= 1.f + 0.010f * (2.f * hashf (n, 12.9898f, 78.233f) - 1.f);
      if (f > 0.45f * _sr) f = 0.45f * _sr;
      _f[n] = f;
      _w[n] = 2.f * std::cos (6.2831853f * f / _sr);
      _base_gain[n] = 1.f / (1.f + f / 1500.f);
      // alternate modes near-hard L/R (interleaved in frequency) -> decorrelated + balanced
      bool left = (n & 1) == 0;
      _pan_l[n] = left ? 0.98f : 0.20f;
      _pan_r[n] = left ? 0.20f : 0.98f;
   }
   set_low_pass_freq (5500.f);
   _decay = 0.6f; update_decay ();
   _macro = 0.6f; update_gains ();
   reset ();
}

}  // namespace dsp
