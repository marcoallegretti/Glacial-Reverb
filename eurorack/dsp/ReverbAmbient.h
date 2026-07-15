/*****************************************************************************

     ReverbAmbient.h
     Ambient: 4-line Hadamard FDN with LFO-modulated delays.

*Tab=3***********************************************************************/

#pragma once

#include "ReverbModel.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class ReverbAmbient : public ReverbModel
{
public:
   explicit       ReverbAmbient (float sample_freq);

   void           set_decay (float d) override { _gain = 0.55f + clamp01 (d) * 0.43f; }
   void           set_macro (float fx) override { _mod = clamp01 (fx); }
   void           set_low_pass_freq (float f) override
   {
      _damp = std::exp (-6.2831853f * f / _sr);
      if (_damp < 0.f) _damp = 0.f;
      if (_damp > 0.95f) _damp = 0.95f;
   }

   void           reset () override
   {
      auto & B = *_buf;
      B.d0.fill (0.f); B.d1.fill (0.f); B.d2.fill (0.f); B.d3.fill (0.f);
      B.ap0.fill (0.f); B.ap1.fill (0.f); B.ap2.fill (0.f); B.ap3.fill (0.f);
      _wp = 0; _ap_wp = 0;
      _lp0 = _lp1 = _lp2 = _lp3 = 0.f;
      _lfo[0] = 0.f; _lfo[1] = 1.6f; _lfo[2] = 3.1f; _lfo[3] = 4.7f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      auto & B = *_buf;

      float x = diffuse (0.5f * (in.left + in.right));

      float depth = _mod * 34.f;
      float a = read (B.d0, float (L0) + depth * std::sin (_lfo[0]));
      float b = read (B.d1, float (L1) + depth * std::sin (_lfo[1]));
      float c = read (B.d2, float (L2) + depth * std::sin (_lfo[2]));
      float d = read (B.d3, float (L3) + depth * std::sin (_lfo[3]));

      _lp0 += (1.f - _damp) * (a - _lp0); a = _lp0;
      _lp1 += (1.f - _damp) * (b - _lp1); b = _lp1;
      _lp2 += (1.f - _damp) * (c - _lp2); c = _lp2;
      _lp3 += (1.f - _damp) * (d - _lp3); d = _lp3;

      float s0 = 0.5f * ( a + b + c + d);
      float s1 = 0.5f * ( a - b + c - d);
      float s2 = 0.5f * ( a + b - c - d);
      float s3 = 0.5f * ( a - b - c + d);

      B.d0 [_wp] = soft (x + _gain * s0);   // bounds resonant buildup
      B.d1 [_wp] = soft (x + _gain * s1);
      B.d2 [_wp] = soft (x + _gain * s2);
      B.d3 [_wp] = soft (x + _gain * s3);

      if (++_wp >= N) _wp = 0;

      for (int i = 0 ; i < 4 ; ++i)
      {
         _lfo[i] += _lfo_inc[i];
         if (_lfo[i] >= 6.2831853f) _lfo[i] -= 6.2831853f;
      }

      return { 0.5f * (a + c), 0.5f * (b + d) };
   }

private:
   static constexpr int N  = 8192;                             // power of two: masked indexing
   static constexpr int AN = 768;
   static constexpr int L0 = 1531, L1 = 2803, L2 = 3719, L3 = 4507;   // mutually prime lengths

   struct Buffers
   {
      std::array<float,N>  d0{}, d1{}, d2{}, d3{};
      std::array<float,AN> ap0{}, ap1{}, ap2{}, ap3{};
   };

   static float   soft (float x)
   {
      if (x < -1.5f) return -1.f;
      if (x >  1.5f) return  1.f;
      return x - x * x * x * (1.f / 6.75f);   // knee at 1.5 -> 1.0 with zero slope (C1 continuous)
   }

   float          diffuse (float x)
   {
      auto & B = *_buf;
      static constexpr int AL[4] = { 211, 367, 541, 727 };
      std::array<float,AN> * ap[4] = { &B.ap0, &B.ap1, &B.ap2, &B.ap3 };
      for (int i = 0 ; i < 4 ; ++i)
      {
         int rp = _ap_wp - AL[i]; if (rp < 0) rp += AN;
         float bufv = (*ap[i]) [rp];
         float y = -0.6f * x + bufv;
         (*ap[i]) [_ap_wp] = x + 0.6f * y;
         x = y;
      }
      if (++_ap_wp >= AN) _ap_wp = 0;
      return x;
   }

   float          read (const std::array<float,N> & b, float delay) const
   {
      float rp = float (_wp) - delay;
      while (rp < 0.f) rp += float (N);
      while (rp >= float (N)) rp -= float (N);   // float wrap can round up to exactly N
      int i1 = int (rp); float f = rp - float (i1);
      int i0 = i1 - 1; if (i0 < 0) i0 += N;
      int i2 = i1 + 1; if (i2 >= N) i2 -= N;
      int i3 = i1 + 2; if (i3 >= N) i3 -= N;
      float y0 = b[i0], y1 = b[i1], y2 = b[i2], y3 = b[i3];
      float c1 = 0.5f*(y2-y0), c2 = y0 - 2.5f*y1 + 2.f*y2 - 0.5f*y3, c3 = 0.5f*(y3-y0)+1.5f*(y1-y2);
      return ((c3*f + c2)*f + c1)*f + y1;
   }

   float          _sr;
   erb::SdramPtr<Buffers> _buf;
   float          _gain = 0.85f, _mod = 0.f, _damp = 0.4f;
   float          _lp0 = 0.f, _lp1 = 0.f, _lp2 = 0.f, _lp3 = 0.f;
   float          _lfo [4] {};
   float          _lfo_inc [4] {};
   int            _wp = 0, _ap_wp = 0;
};

inline ReverbAmbient::ReverbAmbient (float sample_freq)
:  _sr (sample_freq)
,  _buf (erb::make_sdram<Buffers> ())
{
   const float two_pi = 6.2831853f;
   _lfo_inc[0] = two_pi * 0.11f / _sr;
   _lfo_inc[1] = two_pi * 0.17f / _sr;
   _lfo_inc[2] = two_pi * 0.13f / _sr;
   _lfo_inc[3] = two_pi * 0.19f / _sr;
   set_low_pass_freq (6000.f);
   reset ();
}

}  // namespace dsp
