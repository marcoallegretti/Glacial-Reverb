/*****************************************************************************

     ReverbPlateDisp.h
     Plate (dispersive): diffused short-delay Hadamard FDN with dispersion.

*Tab=3***********************************************************************/

#pragma once

#include "ReverbModel.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class ReverbPlateDisp : public ReverbModel
{
public:
   explicit       ReverbPlateDisp (float sample_freq);

   void           set_decay (float d) override { _gain = 0.5f + clamp01 (d) * 0.46f; }
   void           set_macro (float fx) override { _disp = 0.4f + clamp01 (fx) * 0.4f; }
   void           set_low_pass_freq (float f) override
   {
      _damp = std::exp (-6.2831853f * f / _sr);
      if (_damp < 0.f) _damp = 0.f;
      if (_damp > 0.9f) _damp = 0.9f;
   }

   void           reset () override
   {
      auto & B = *_buf;
      B.d0.fill (0.f); B.d1.fill (0.f); B.d2.fill (0.f); B.d3.fill (0.f);
      for (auto & a : B.ap) a.fill (0.f);
      for (auto & a : B.di) a.fill (0.f);
      _wp = 0; _lp0 = _lp1 = _lp2 = _lp3 = 0.f;
      _lfo[0] = 0.f; _lfo[1] = 1.9f; _lfo[2] = 3.6f; _lfo[3] = 5.1f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      auto & B = *_buf;
      float x = diffuse (0.5f * (in.left + in.right));

      float md = 2.5f;
      float a = read (B.d0, float (L0) + md * std::sin (_lfo[0]));
      float b = read (B.d1, float (L1) + md * std::sin (_lfo[1]));
      float c = read (B.d2, float (L2) + md * std::sin (_lfo[2]));
      float d = read (B.d3, float (L3) + md * std::sin (_lfo[3]));

      a = ap1 (B.ap[0], KA0, a); b = ap1 (B.ap[1], KA1, b);
      c = ap1 (B.ap[2], KA2, c); d = ap1 (B.ap[3], KA3, d);

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
      return { 0.5f * (a + c), 0.5f * (b + d) };
   }

private:
   static constexpr unsigned D = 2048, DMASK = 2047;
   static constexpr unsigned AP = 128, AMASK = 127;
   static constexpr unsigned DI = 512, DIMASK = 511;
   static constexpr int L0 = 787, L1 = 1123, L2 = 1451, L3 = 1789;
   static constexpr int KA0 = 37, KA1 = 43, KA2 = 53, KA3 = 61;

   struct Buffers
   {
      std::array<float,D>                d0{}, d1{}, d2{}, d3{};
      std::array<std::array<float,AP>,4> ap{};
      std::array<std::array<float,DI>,4> di{};
   };

   static float   soft (float x)
   {
      if (x < -1.5f) return -1.f;
      if (x >  1.5f) return  1.f;
      return x - x * x * x * (1.f / 6.75f);
   }

   float          ap1 (std::array<float,AP> & buf, int k, float x)
   {
      float wK = buf [(_wp - unsigned (k)) & AMASK];
      float w = x - _disp * wK;
      buf [_wp & AMASK] = w;
      return _disp * w + wK;
   }

   float          diffuse (float x)
   {
      auto & B = *_buf;
      static constexpr int DL[4] = { 89, 131, 181, 239 };
      for (int i = 0 ; i < 4 ; ++i)
      {
         auto & buf = B.di[i];
         float dv = buf [(_wp - unsigned (DL[i])) & DIMASK];
         float y = -0.7f * x + dv;
         buf [_wp & DIMASK] = x + 0.7f * y;
         x = y;
      }
      return x;
   }

   float          read (const std::array<float,D> & b, float delay) const
   {
      float rp = float (_wp) - delay;
      while (rp < 0.f) rp += float (D);
      int i1 = int (rp) & int (DMASK); float f = rp - std::floor (rp);
      int i0 = (i1 - 1) & int (DMASK), i2 = (i1 + 1) & int (DMASK), i3 = (i1 + 2) & int (DMASK);
      float y0 = b[i0], y1 = b[i1], y2 = b[i2], y3 = b[i3];
      float c1 = 0.5f*(y2-y0), c2 = y0 - 2.5f*y1 + 2.f*y2 - 0.5f*y3, c3 = 0.5f*(y3-y0)+1.5f*(y1-y2);
      return ((c3*f + c2)*f + c1)*f + y1;
   }

   float          _sr;
   erb::SdramPtr<Buffers> _buf;
   float          _gain = 0.85f, _disp = 0.6f, _damp = 0.3f;
   float          _lp0 = 0.f, _lp1 = 0.f, _lp2 = 0.f, _lp3 = 0.f;
   float          _lfo [4] {}, _lfo_inc [4] {};
   unsigned       _wp = 0;
};

inline ReverbPlateDisp::ReverbPlateDisp (float sample_freq)
:  _sr (sample_freq)
,  _buf (erb::make_sdram<Buffers> ())
{
   const float two_pi = 6.2831853f;
   _lfo_inc[0] = two_pi * 0.7f / _sr;
   _lfo_inc[1] = two_pi * 1.1f / _sr;
   _lfo_inc[2] = two_pi * 0.9f / _sr;
   _lfo_inc[3] = two_pi * 1.3f / _sr;
   set_low_pass_freq (8500.f);
   reset ();
}

}  // namespace dsp
