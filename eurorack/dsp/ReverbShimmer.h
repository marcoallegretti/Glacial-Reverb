/*****************************************************************************

     ReverbShimmer.h
     Shimmer: octave-up grain pitch shift inside the tank feedback.

*Tab=3***********************************************************************/

#pragma once

#include "ReverbModel.h"
#include "ReverbSc.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class ReverbShimmer : public ReverbModel
{
public:
   explicit       ReverbShimmer (float sample_freq);

   void           set_decay (float d) override
   {
      d = clamp01 (d);
      _tank.set_feedback (0.3f + d * 0.45f);
   }

   void           set_macro (float fx) override { _shimmer = clamp01 (fx); }
   void           set_low_pass_freq (float f) override { _tank.set_low_pass_freq (f); }

   void           reset () override
   {
      _tank.reset ();
      auto & B = *_buf; B.l.fill (0.f); B.r.fill (0.f);
      _wp = 0; _phase = 0.f; _phase_r = 0.5f;
      _fb_l = _fb_r = _lp_l = _lp_r = 0.f;
      _hp_l = _hp_r = _hx_l = _hx_r = 0.f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      // cross-couple: L/R must not collapse to mono
      float g = _shimmer * 0.4f;
      StereoFrame tin { in.left + g * _fb_r, in.right + g * _fb_l };
      auto w = _tank.process ({ tin.left, tin.right });

      auto & B = *_buf;
      B.l [_wp] = w.left;
      B.r [_wp] = w.right;

      float sl = grain (B.l, _phase);
      float sr = grain (B.r, _phase_r);

      _phase   += _inc;          if (_phase   >= 1.f) _phase   -= 1.f;
      _phase_r += _inc * 0.981f; if (_phase_r >= 1.f) _phase_r -= 1.f;
      if (++_wp >= N) _wp = 0;

      // load-bearing: without it the grain feedback pushes loop DC gain over 1
      // and the tank latches to a DC rail at high FX/DECAY. Do not remove.
      float hl = sl - _hx_l + 0.9985f * _hp_l; _hx_l = sl; _hp_l = hl;
      float hr = sr - _hx_r + 0.9985f * _hp_r; _hx_r = sr; _hp_r = hr;

      // lowpass + soft-clip keep the loop gain < 1
      _lp_l += 0.35f * (hl - _lp_l);
      _lp_r += 0.35f * (hr - _lp_r);
      _fb_l = soft (_lp_l);
      _fb_r = soft (_lp_r);

      return { w.left, w.right };
   }

private:
   static constexpr int N = 6144;

   struct Buffers { std::array<float,N> l{}; std::array<float,N> r{}; };

   static float   soft (float x)
   {
      if (x < -1.f) return -2.f/3.f;
      if (x >  1.f) return  2.f/3.f;
      return x - x * x * x * (1.f/3.f);
   }

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
   ReverbSc       _tank;
   erb::SdramPtr<Buffers> _buf;
   float          _shimmer = 0.f;
   float          _len = 0.f, _inc = 0.f, _phase = 0.f, _phase_r = 0.5f;
   int            _wp = 0;
   float          _fb_l = 0.f, _fb_r = 0.f, _lp_l = 0.f, _lp_r = 0.f;
   float          _hp_l = 0.f, _hp_r = 0.f, _hx_l = 0.f, _hx_r = 0.f;
};

inline ReverbShimmer::ReverbShimmer (float sample_freq)
:  _sr (sample_freq)
,  _tank (sample_freq)
,  _buf (erb::make_sdram<Buffers> ())
{
   _len = 0.045f * _sr;
   if (_len > float (N - 4)) _len = float (N - 4);
   _inc = 1.f / _len;               // ratio 2 = octave up
   _tank.set_low_pass_freq (9000.f);
   reset ();
}

}  // namespace dsp
