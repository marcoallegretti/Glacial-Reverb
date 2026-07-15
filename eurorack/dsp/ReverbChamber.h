/*****************************************************************************

     ReverbChamber.h
     Chamber: Moorer early-reflection tap bank feeding a Hadamard FDN.

*Tab=3***********************************************************************/

#pragma once

#include "ReverbModel.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class ReverbChamber : public ReverbModel
{
public:
   explicit       ReverbChamber (float sample_freq);

   void           set_decay (float d) override { _gain = 0.5f + clamp01 (d) * 0.47f; }

   // macro = distance: near is dominated by the tap pattern, far by the tank
   void           set_macro (float fx) override
   {
      float m = clamp01 (fx);
      _er_lvl   = 1.f - 0.55f * m;
      _late_lvl = 0.35f + 0.75f * m;
   }

   void           set_low_pass_freq (float f) override
   {
      _damp = std::exp (-6.2831853f * f / _sr);
      if (_damp < 0.f) _damp = 0.f;
      if (_damp > 0.9f) _damp = 0.9f;
   }

   void           reset () override
   {
      auto & B = *_buf;
      B.er.fill (0.f);
      B.d0.fill (0.f); B.d1.fill (0.f); B.d2.fill (0.f); B.d3.fill (0.f);
      _wp = 0; _lp0 = _lp1 = _lp2 = _lp3 = 0.f; _in_lp = 0.f;
      _lfo[0] = 0.f; _lfo[1] = 2.1f; _lfo[2] = 3.9f; _lfo[3] = 5.4f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      auto & B = *_buf;

      // air absorption on the whole tap bank rather than per tap
      _in_lp += 0.45f * (0.5f * (in.left + in.right) - _in_lp);
      B.er [_wp & EMASK] = _in_lp;

      float el = 0.f, er = 0.f;
      for (int i = 0 ; i < NTAP ; ++i)
      {
         el += tap (B.er, _off_l[i], _frc_l[i]) * ER_G[i];
         er += tap (B.er, _off_r[i], _frc_r[i]) * ER_G[i];
      }
      el *= ER_TRIM; er *= ER_TRIM;

      // the taps run at ~240 echoes/s, far under the ~1000/s that reads as
      // diffuse, so the tank is excited by the taps and not by the dry input:
      // every tap seeds it and the density comes out of the recirculation
      float x = 0.5f * (el + er);

      float md = 3.0f;
      float a = read (B.d0, float (L0) + md * std::sin (_lfo[0]));
      float b = read (B.d1, float (L1) + md * std::sin (_lfo[1]));
      float c = read (B.d2, float (L2) + md * std::sin (_lfo[2]));
      float d = read (B.d3, float (L3) + md * std::sin (_lfo[3]));

      _lp0 += (1.f - _damp) * (a - _lp0); a = _lp0;
      _lp1 += (1.f - _damp) * (b - _lp1); b = _lp1;
      _lp2 += (1.f - _damp) * (c - _lp2); c = _lp2;
      _lp3 += (1.f - _damp) * (d - _lp3); d = _lp3;

      float s0 = 0.5f * ( a + b + c + d);
      float s1 = 0.5f * ( a - b + c - d);
      float s2 = 0.5f * ( a + b - c - d);
      float s3 = 0.5f * ( a - b - c + d);

      B.d0 [_wp & DMASK] = soft (x + _gain * s0);
      B.d1 [_wp & DMASK] = soft (x + _gain * s1);
      B.d2 [_wp & DMASK] = soft (x + _gain * s2);
      B.d3 [_wp & DMASK] = soft (x + _gain * s3);

      ++_wp;
      for (int i = 0 ; i < 4 ; ++i)
      {
         _lfo[i] += _lfo_inc[i];
         if (_lfo[i] >= 6.2831853f) _lfo[i] -= 6.2831853f;
      }

      float ll = 0.5f * (a + c), lr = 0.5f * (b + d);
      return { (el * _er_lvl + ll * _late_lvl) * NORM,
               (er * _er_lvl + lr * _late_lvl) * NORM };
   }

private:
   static constexpr int      NTAP  = 17;
   static constexpr unsigned E     = 4096, EMASK = 4095;
   static constexpr unsigned D     = 2048, DMASK = 2047;
   static constexpr int      L0 = 1327, L1 = 1481, L2 = 1699, L3 = 1933;

   // Moorer 1979, Boston Symphony Hall geometric model (table 3), scaled to a
   // chamber. Clustered, not evenly spaced: the clusters are the wall-pair
   // geometry and flattening them costs the room its signature.
   static constexpr float SIZE    = 0.55f;    // hall span 79.7 ms -> ~44 ms
   static constexpr float SPREAD  = 1.02f;    // R delays only: decorrelates L/R
   static constexpr float ER_TRIM = 0.30f;    // taps sum to +15.9 dB peak
   static constexpr float NORM    = 1.9f;     // match the other models' level

   static constexpr float ER_MS [NTAP] = {
       4.3f, 21.5f, 22.5f, 26.8f, 27.0f, 29.8f, 45.8f, 48.5f, 57.2f,
      59.5f, 61.2f, 70.7f, 70.8f, 72.6f, 74.1f, 75.3f, 79.7f
   };
   static constexpr float ER_G [NTAP] = {
      0.841f, 0.504f, 0.491f, 0.379f, 0.380f, 0.346f, 0.289f, 0.272f, 0.192f,
      0.217f, 0.181f, 0.180f, 0.181f, 0.176f, 0.142f, 0.167f, 0.134f
   };

   struct Buffers
   {
      std::array<float,E> er{};
      std::array<float,D> d0{}, d1{}, d2{}, d3{};
   };

   static float   soft (float x)
   {
      if (x < -1.5f) return -1.f;
      if (x >  1.5f) return  1.f;
      return x - x * x * x * (1.f / 6.75f);
   }

   // taps are fixed geometry, so the integer/fraction split is precomputed and
   // linear interpolation is enough: nothing modulates them
   float          tap (const std::array<float,E> & b, int off, float frc) const
   {
      unsigned i0 = (_wp - unsigned (off)) & EMASK;
      unsigned i1 = (i0 - 1u) & EMASK;
      return b[i0] + (b[i1] - b[i0]) * frc;
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

   float          _sr;
   erb::SdramPtr<Buffers> _buf;
   int            _off_l [NTAP] {}, _off_r [NTAP] {};
   float          _frc_l [NTAP] {}, _frc_r [NTAP] {};
   float          _gain = 0.85f, _damp = 0.3f;
   float          _er_lvl = 0.72f, _late_lvl = 0.72f;
   float          _in_lp = 0.f;
   float          _lp0 = 0.f, _lp1 = 0.f, _lp2 = 0.f, _lp3 = 0.f;
   float          _lfo [4] {}, _lfo_inc [4] {};
   unsigned       _wp = 0;
};

inline ReverbChamber::ReverbChamber (float sample_freq)
:  _sr (sample_freq)
,  _buf (erb::make_sdram<Buffers> ())
{
   const float spl_per_ms = _sr * 0.001f;
   for (int i = 0 ; i < NTAP ; ++i)
   {
      float dl = ER_MS[i] * SIZE * spl_per_ms;
      float dr = dl * SPREAD;
      float lim = float (E - 2);
      if (dl > lim) dl = lim;
      if (dr > lim) dr = lim;
      _off_l[i] = int (dl); _frc_l[i] = dl - float (_off_l[i]);
      _off_r[i] = int (dr); _frc_r[i] = dr - float (_off_r[i]);
   }

   const float two_pi = 6.2831853f;
   _lfo_inc[0] = two_pi * 0.53f / _sr;
   _lfo_inc[1] = two_pi * 0.81f / _sr;
   _lfo_inc[2] = two_pi * 0.67f / _sr;
   _lfo_inc[3] = two_pi * 0.95f / _sr;
   set_low_pass_freq (7000.f);
   reset ();
}

}  // namespace dsp
