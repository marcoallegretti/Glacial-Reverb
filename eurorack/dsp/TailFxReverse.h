/*****************************************************************************

     TailFxReverse.h
     Reverse: backwards-swell grains.

*Tab=3***********************************************************************/

#pragma once

#include "TailFx.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class TailFxReverse : public TailFx
{
public:
   explicit       TailFxReverse (float sample_freq);

   void           set_amount (float a) override { _amount = clamp01 (a); }

   void           reset () override
   {
      auto & B = *_buf; B.l.fill (0.f); B.r.fill (0.f);
      _wp = 0;
      _g[0].age = 0;     _g[0].start = 0;
      _g[1].age = _half; _g[1].start = 0;
   }

   StereoFrame    process (StereoFrame in) override
   {
      auto & B = *_buf;
      B.l [_wp] = in.left;
      B.r [_wp] = in.right;

      float out_l = 0.f, out_r = 0.f;
      for (auto & g : _g)
      {
         if (g.age >= _len) { g.age = 0; g.start = _wp; }
         float p   = float (g.age) / float (_len);
         float win = 0.5f - 0.5f * std::cos (6.2831853f * p);
         float pos = float (g.start) - float (g.age);
         out_l += read (B.l, pos)          * win;
         out_r += read (B.r, pos - _roff)  * win;
         ++g.age;
      }
      if (++_wp >= N) _wp = 0;

      float g = _amount * 0.9f;
      return { in.left + g * out_l, in.right + g * out_r };
   }

private:
   static constexpr int N = 96000;

   struct Grain { int start = 0; int age = 0; };
   struct Buffers { std::array<float,N> l{}, r{}; };

   float          read (const std::array<float,N> & b, float pos) const
   {
      while (pos < 0.f) pos += float (N);
      while (pos >= float (N)) pos -= float (N);
      int i1 = int (pos); float f = pos - float (i1);
      int i0 = i1 - 1; if (i0 < 0) i0 += N;
      int i2 = i1 + 1; if (i2 >= N) i2 -= N;
      int i3 = i1 + 2; if (i3 >= N) i3 -= N;
      float y0 = b[i0], y1 = b[i1], y2 = b[i2], y3 = b[i3];
      float c1 = 0.5f*(y2-y0), c2 = y0 - 2.5f*y1 + 2.f*y2 - 0.5f*y3, c3 = 0.5f*(y3-y0)+1.5f*(y1-y2);
      return ((c3*f + c2)*f + c1)*f + y1;
   }

   float          _sr;
   erb::SdramPtr<Buffers> _buf;
   Grain          _g [2];
   float          _amount = 0.f, _roff = 0.f;
   int            _wp = 0, _len = 0, _half = 0;
};

inline TailFxReverse::TailFxReverse (float sample_freq)
:  _sr (sample_freq)
,  _buf (erb::make_sdram<Buffers> ())
{
   _len  = int (0.40f * _sr);
   _half = _len / 2;
   _roff = 0.013f * _sr;
   reset ();
}

}  // namespace dsp
