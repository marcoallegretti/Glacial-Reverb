/*****************************************************************************

     TailFxChorus.h
     Chorus: LFO-modulated detune voices.

*Tab=3***********************************************************************/

#pragma once

#include "TailFx.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class TailFxChorus : public TailFx
{
public:
   explicit       TailFxChorus (float sample_freq);

   void           set_amount (float a) override { _amount = clamp01 (a); }

   void           reset () override
   {
      auto & B = *_buf; B.l.fill (0.f); B.r.fill (0.f);
      _wp = 0;
      _lfo[0] = 0.f; _lfo[1] = 1.7f; _lfo[2] = 3.4f; _lfo[3] = 5.0f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      auto & B = *_buf;
      B.l [_wp] = in.left;
      B.r [_wp] = in.right;

      float depth = (0.4f + 0.6f * _amount) * _mod_spl;
      float vl = read (B.l, _base + depth * std::sin (_lfo[0]))
               + read (B.l, _base + depth * std::sin (_lfo[1]));
      float vr = read (B.r, _base + depth * std::sin (_lfo[2]))
               + read (B.r, _base + depth * std::sin (_lfo[3]));

      for (int i = 0 ; i < 4 ; ++i)
      {
         _lfo[i] += _lfo_inc[i];
         if (_lfo[i] >= 6.2831853f) _lfo[i] -= 6.2831853f;
      }
      if (++_wp >= N) _wp = 0;

      float g = _amount * 0.5f;
      return { in.left + g * vl, in.right + g * vr };
   }

private:
   static constexpr int N = 2048;

   struct Buffers { std::array<float,N> l{}, r{}; };

   float          read (const std::array<float,N> & b, float delay) const
   {
      float rp = float (_wp) - delay;
      while (rp < 0.f) rp += float (N);
      while (rp >= float (N)) rp -= float (N);   // float wrap can round up to exactly N
      int i0 = int (rp); float f = rp - float (i0);
      int im1 = i0 - 1; if (im1 < 0) im1 += N;
      int i1 = i0 + 1; if (i1 >= N) i1 -= N;
      int i2 = i0 + 2; if (i2 >= N) i2 -= N;
      float y0 = b[im1], y1 = b[i0], y2 = b[i1], y3 = b[i2];
      float c1 = 0.5f*(y2-y0), c2 = y0 - 2.5f*y1 + 2.f*y2 - 0.5f*y3, c3 = 0.5f*(y3-y0)+1.5f*(y1-y2);
      return ((c3*f + c2)*f + c1)*f + y1;
   }

   float          _sr;
   erb::SdramPtr<Buffers> _buf;
   float          _amount = 0.f;
   float          _base = 0.f, _mod_spl = 0.f;
   float          _lfo [4] {};
   float          _lfo_inc [4] {};
   int            _wp = 0;
};

inline TailFxChorus::TailFxChorus (float sample_freq)
:  _sr (sample_freq)
,  _buf (erb::make_sdram<Buffers> ())
{
   _base    = 0.015f * _sr;
   _mod_spl = 0.005f * _sr;
   const float two_pi = 6.2831853f;
   _lfo_inc[0] = two_pi * 0.31f / _sr;
   _lfo_inc[1] = two_pi * 0.47f / _sr;
   _lfo_inc[2] = two_pi * 0.37f / _sr;
   _lfo_inc[3] = two_pi * 0.53f / _sr;
   reset ();
}

}  // namespace dsp
