/*****************************************************************************

     TailFxPitch.h
     Pitch: octave-up grain shimmer.

*Tab=3***********************************************************************/

#pragma once

#include "TailFx.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class TailFxPitch : public TailFx
{
public:
   explicit       TailFxPitch (float sample_freq);

   void           set_amount (float a) override { _amount = clamp01 (a); }

   void           reset () override
   {
      auto & B = *_buf; B.l.fill (0.f); B.r.fill (0.f);
      _wp = 0; _phase = 0.f; _phase_r = 0.5f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      auto & B = *_buf;
      B.l [_wp] = in.left;
      B.r [_wp] = in.right;

      float sl = grain (B.l, _phase);
      float sr = grain (B.r, _phase_r);

      _phase   += _inc;          if (_phase   >= 1.f) _phase   -= 1.f;
      _phase_r += _inc * 0.987f; if (_phase_r >= 1.f) _phase_r -= 1.f;
      if (++_wp >= N) _wp = 0;

      float g = _amount * 0.9f;
      return { in.left + g * sl, in.right + g * sr };
   }

private:
   static constexpr int N = 6144;

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

   float          grain (const std::array<float,N> & b, float phase) const
   {
      if (phase >= 1.f) phase -= 1.f;
      float p2 = phase + 0.5f; if (p2 >= 1.f) p2 -= 1.f;
      float w1 = 0.5f - 0.5f * std::cos (6.2831853f * phase);
      float w2 = 0.5f - 0.5f * std::cos (6.2831853f * p2);
      return read (b, (1.f - phase) * _len) * w1 + read (b, (1.f - p2) * _len) * w2;
   }

   float          _sr;
   erb::SdramPtr<Buffers> _buf;
   float          _amount = 0.f;
   float          _len = 0.f, _inc = 0.f, _phase = 0.f, _phase_r = 0.5f;
   int            _wp = 0;
};

inline TailFxPitch::TailFxPitch (float sample_freq)
:  _sr (sample_freq)
,  _buf (erb::make_sdram<Buffers> ())
{
   _len = 0.045f * _sr;
   if (_len > float (N - 4)) _len = float (N - 4);
   _inc = 1.f / _len;
   reset ();
}

}  // namespace dsp
