/*****************************************************************************

     ReverbSpringDisp.h
     Spring (dispersive waveguide): stretched-allpass chirp in a feedback delay.

*Tab=3***********************************************************************/

#pragma once

#include "ReverbModel.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class ReverbSpringDisp : public ReverbModel
{
public:
   explicit       ReverbSpringDisp (float sample_freq);

   void           set_decay (float d) override { _gain = 0.6f + clamp01 (d) * 0.35f; }
   void           set_macro (float fx) override { _disp = 0.5f + clamp01 (fx) * 0.3f; }
   void           set_low_pass_freq (float f) override
   {
      _damp = std::exp (-6.2831853f * f / _sr);
      if (_damp < 0.f) _damp = 0.f;
      if (_damp > 0.9f) _damp = 0.9f;
   }

   void           reset () override
   {
      auto & B = *_buf;
      B.m0.fill (0.f); B.m1.fill (0.f);
      for (auto & a : B.a0) a.fill (0.f);
      for (auto & a : B.a1) a.fill (0.f);
      _wp = 0; _lp0 = _lp1 = 0.f; _hp0 = _hp1 = _hx0 = _hx1 = 0.f;
      _lfo = 0.f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      auto & B = *_buf;
      float x = 0.5f * (in.left + in.right);

      float mod = 3.5f * std::sin (_lfo);
      _lfo += _lfo_inc; if (_lfo >= 6.2831853f) _lfo -= 6.2831853f;

      float o0 = spring (B.m0, B.a0, LM0, K0, _lp0, _hp0, _hx0, x, mod);
      float o1 = spring (B.m1, B.a1, LM1, K1, _lp1, _hp1, _hx1, x, -mod);

      ++_wp;
      return { o0, o1 };
   }

private:
   static constexpr unsigned MAIN = 4096, MMASK = 4095;
   static constexpr unsigned AP = 128, AMASK = 127;
   static constexpr int NAP = 8;
   static constexpr int LM0 = 1399, LM1 = 1693;
   static constexpr int K0 = 40, K1 = 44;

   struct Buffers
   {
      std::array<float,MAIN>               m0{}, m1{};
      std::array<std::array<float,AP>,NAP> a0{}, a1{};
   };

   static float   soft (float x)
   {
      if (x < -1.5f) return -1.f;
      if (x >  1.5f) return  1.f;
      return x - x * x * x * (1.f / 6.75f);
   }

   float          read_frac (const std::array<float,MAIN> & b, float delay) const
   {
      float rp = float (_wp) - delay;
      while (rp < 0.f) rp += float (MAIN);
      int i1 = int (rp) & int (MMASK); float f = rp - std::floor (rp);
      int i0 = (i1 - 1) & int (MMASK), i2 = (i1 + 1) & int (MMASK), i3 = (i1 + 2) & int (MMASK);
      float y0 = b[i0], y1 = b[i1], y2 = b[i2], y3 = b[i3];
      float c1 = 0.5f*(y2-y0), c2 = y0 - 2.5f*y1 + 2.f*y2 - 0.5f*y3, c3 = 0.5f*(y3-y0)+1.5f*(y1-y2);
      return ((c3*f + c2)*f + c1)*f + y1;
   }

   float          disperse (std::array<std::array<float,AP>,NAP> & ap, int k, float x)
   {
      for (int m = 0 ; m < NAP ; ++m)
      {
         auto & buf = ap[m];
         float wK = buf [(_wp - unsigned (k)) & AMASK];
         float w = x - _disp * wK;
         buf [_wp & AMASK] = w;
         x = _disp * w + wK;
      }
      return x;
   }

   float          spring (std::array<float,MAIN> & main, std::array<std::array<float,AP>,NAP> & ap,
                          int lm, int k, float & lp, float & hp, float & hx, float x, float mod)
   {
      float delayed = read_frac (main, float (lm) + mod);
      float v = disperse (ap, k, delayed);
      lp += (1.f - _damp) * (v - lp);
      float hpv = lp - hx + 0.9975f * hp; hx = lp; hp = hpv;
      main [_wp & MMASK] = soft (x + _gain * hpv);
      return hpv;                                    // output the dispersed signal -> every echo chirps
   }

   float          _sr;
   erb::SdramPtr<Buffers> _buf;
   float          _gain = 0.85f, _disp = 0.65f, _damp = 0.4f;
   float          _lp0 = 0.f, _lp1 = 0.f;
   float          _hp0 = 0.f, _hp1 = 0.f, _hx0 = 0.f, _hx1 = 0.f;
   float          _lfo = 0.f, _lfo_inc = 0.f;
   unsigned       _wp = 0;
};

inline ReverbSpringDisp::ReverbSpringDisp (float sample_freq)
:  _sr (sample_freq)
,  _buf (erb::make_sdram<Buffers> ())
{
   _lfo_inc = 6.2831853f * 0.6f / _sr;
   set_low_pass_freq (4000.f);
   reset ();
}

}  // namespace dsp
