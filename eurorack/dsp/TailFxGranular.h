/*****************************************************************************

     TailFxGranular.h
     Granular: diffuse grain smear.

*Tab=3***********************************************************************/

#pragma once

#include "TailFx.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class TailFxGranular : public TailFx
{
public:
   explicit       TailFxGranular (float sample_freq);

   void           set_amount (float a) override { _amount = clamp01 (a); }

   void           reset () override
   {
      auto & B = *_buf; B.l.fill (0.f); B.r.fill (0.f);
      _wp = 0; _spawn = 0;
      for (auto & g : _grains) g.age = g.dur = 0;
      _rng = 3131u;
   }

   StereoFrame    process (StereoFrame in) override
   {
      auto & B = *_buf;
      B.l [_wp] = in.left;
      B.r [_wp] = in.right;

      int want = 1 + int (_amount * float (MAX_GRAINS - 1) + 0.5f);
      int live = 0;
      for (auto & g : _grains) if (g.dur > 0) ++live;

      bool spawn_now = false;
      if (--_spawn <= 0) { _spawn = _spawn_period; if (live < want) spawn_now = true; }

      float out_l = 0.f, out_r = 0.f;
      for (auto & g : _grains)
      {
         if (g.dur <= 0)
         {
            if (spawn_now) { spawn (g); spawn_now = false; } else continue;
         }
         float p = float (g.age) / float (g.dur);
         float win = 0.5f - 0.5f * std::cos (6.2831853f * p);
         float pos = float (g.start) + float (g.age);
         out_l += read (B.l, pos)          * win * g.pan_l;
         out_r += read (B.r, pos - g.roff) * win * g.pan_r;
         if (++g.age >= g.dur) g.dur = 0;
      }
      if (++_wp >= N) _wp = 0;

      float norm = 1.4f / std::sqrt (float (want > 0 ? want : 1));
      float g = _amount * norm;
      return { in.left + g * out_l, in.right + g * out_r };
   }

private:
   static constexpr int N = 48000;
   static constexpr int MAX_GRAINS = 10;

   struct Grain { int start = 0; int age = 0; int dur = 0; float roff = 0.f; float pan_l = 0.7f; float pan_r = 0.7f; };
   struct Buffers { std::array<float,N> l{}, r{}; };

   unsigned       rnd () { _rng = _rng * 1664525u + 1013904223u; return _rng; }
   float          rndf () { return float (rnd () >> 9) * (1.f / 8388608.f); }

   void           spawn (Grain & g)
   {
      g.dur = int ((0.08f + 0.22f * rndf ()) * _sr);
      g.age = 0;
      int back = int ((0.03f + 0.35f * rndf ()) * _sr);
      g.start = _wp - back; while (g.start < 0) g.start += N;
      g.roff = (0.004f + 0.018f * rndf ()) * _sr;
      float pan = 0.5f + 0.45f * (rndf () * 2.f - 1.f);
      g.pan_l = 1.f - pan; g.pan_r = pan;
   }

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
   Grain          _grains [MAX_GRAINS];
   float          _amount = 0.f;
   int            _wp = 0, _spawn = 0, _spawn_period = 0;
   unsigned       _rng = 3131u;
};

inline TailFxGranular::TailFxGranular (float sample_freq)
:  _sr (sample_freq)
,  _buf (erb::make_sdram<Buffers> ())
{
   _spawn_period = int (0.028f * _sr);
   reset ();
}

}  // namespace dsp
