/*****************************************************************************

     ReverbPlateModal.h
     Plate (modal): resonator bank at constant modal density.

*Tab=3***********************************************************************/

#pragma once

#include "ReverbModel.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class ReverbPlateModal : public ReverbModel
{
public:
   explicit       ReverbPlateModal (float sample_freq);

   void           set_decay (float d) override
   {
      d = clamp01 (d);
      if (d == _decay) return;
      _decay = d; update_decay ();
   }

   void           set_macro (float fx) override
   {
      fx = clamp01 (fx);
      if (fx == _macro) return;
      _macro = fx; update_gains ();
   }

   void           set_low_pass_freq (float f) override
   {
      _out_c = 1.f - std::exp (-6.2831853f * f / _sr);
   }

   void           reset () override
   {
      for (int n = 0 ; n < N ; ++n) { _y1[n] = _y2[n] = 0.f; }
      _olp_l = _olp_r = 0.f;
      _dec->fill (0.f); _dwp = 0;
   }

   StereoFrame    process (StereoFrame in) override
   {
      float x = 0.5f * (in.left + in.right);
      float out_l = 0.f, out_r = 0.f;
      for (int n = 0 ; n < N ; ++n)
      {
         float y = _a1[n] * _y1[n] - _a2[n] * _y2[n] + x * _ig[n];
         _y2[n] = _y1[n]; _y1[n] = y;
         out_l += y * _cl[n];
         out_r += y * _cr[n];
      }
      out_l *= NORM; out_r *= NORM;
      _olp_l += _out_c * (out_l - _olp_l);
      _olp_r += _out_c * (out_r - _olp_r);

      // R-only allpass: alternate panning is balanced but correlated
      auto & D = *_dec;
      float dv = D [(_dwp - DEC_K) & DEC_MASK];
      float w = _olp_r - 0.7f * dv;
      D [_dwp & DEC_MASK] = w;
      float r_dec = 0.7f * w + dv;
      _dwp = (_dwp + 1) & DEC_MASK;

      return { _olp_l, r_dec };
   }

private:
   static constexpr int   N = 128;
   static constexpr float NORM = 1.3f;
   static constexpr float F_LO = 120.f, F_HI = 9500.f, F_REF = 700.f;
   static constexpr unsigned DEC_SIZE = 1024, DEC_MASK = 1023;
   static constexpr unsigned DEC_K = 691;

   static float   hashf (int n, float a, float b)
   {
      float s = std::sin (float (n) * a + b) * 43758.5453f;
      return s - std::floor (s);
   }

   void           update_decay ()
   {
      float t60_base = 0.5f + _decay * 3.5f;
      for (int n = 0 ; n < N ; ++n)
      {
         float t60 = t60_base * std::pow (F_REF / _f[n], 0.35f);
         if (t60 < 0.05f) t60 = 0.05f;
         float r = std::exp (-6.9078f / (t60 * _sr));
         float bw = 1.f - _broad[n];
         r *= bw;
         if (r > 0.99993f) r = 0.99993f;
         _a1[n] = _w[n] * r;
         _a2[n] = r * r;
         _ig[n] = 1.f - r;
      }
   }

   void           update_gains ()
   {
      for (int n = 0 ; n < N ; ++n)
      {
         float u = (_f[n] - F_LO) / (F_HI - F_LO);
         float tilt = 0.5f + _macro - u * (1.f - _macro);
         if (tilt < 0.f) tilt = 0.f;
         float g = _base_gain[n] * tilt;
         _cl[n] = g * _pan_l[n];
         _cr[n] = g * _pan_r[n];
      }
   }

   float          _sr;
   float          _decay = -1.f, _macro = -1.f, _out_c = 0.5f;
   float          _w [N] {}, _f [N] {}, _broad [N] {};
   float          _a1 [N] {}, _a2 [N] {}, _ig [N] {};
   float          _base_gain [N] {};
   float          _cl [N] {}, _cr [N] {};
   float          _pan_l [N] {}, _pan_r [N] {};
   float          _y1 [N] {}, _y2 [N] {};
   float          _olp_l = 0.f, _olp_r = 0.f;
   erb::SdramPtr<std::array<float,DEC_SIZE>> _dec;
   unsigned       _dwp = 0;
};

inline ReverbPlateModal::ReverbPlateModal (float sample_freq)
:  _sr (sample_freq)
,  _dec (erb::make_sdram<std::array<float,DEC_SIZE>> ())
{
   for (int n = 0 ; n < N ; ++n)
   {
      float u = (float (n) + 0.5f) / float (N);
      float f = F_LO + (F_HI - F_LO) * u;                        // uniform in Hz = constant modal density
      f += (F_HI - F_LO) / float (N) * (hashf (n, 12.9898f, 78.233f) - 0.5f);
      if (f < 30.f) f = 30.f;
      if (f > 0.45f * _sr) f = 0.45f * _sr;
      _f[n] = f;
      _w[n] = 2.f * std::cos (6.2831853f * f / _sr);
      _base_gain[n] = 1.f / (1.f + f / 2500.f);
      _broad[n] = 0.00004f * (f / 1000.f);
      bool left = (n & 1) == 0;                                  // alternate -> energy-balanced L/R
      _pan_l[n] = left ? 1.0f : 0.10f;
      _pan_r[n] = left ? 0.10f : 1.0f;
   }
   set_low_pass_freq (7500.f);
   _decay = 0.6f; update_decay ();
   _macro = 0.6f; update_gains ();
   reset ();
}

}  // namespace dsp
