/*****************************************************************************

     TailFxResonator.h
     Resonator: damped Karplus combs on a consonant harmonic stack.

*Tab=3***********************************************************************/

#pragma once

#include "TailFx.h"
#include "erb/SdramPtr.h"

#include <array>

namespace dsp
{

class TailFxResonator : public TailFx
{
public:
   explicit       TailFxResonator (float sample_freq);

   void           set_amount (float a) override
   {
      _amount = clamp01 (a);
      _fb = 0.85f + _amount * 0.10f;
   }

   void           reset () override
   {
      auto & B = *_buf;
      for (auto & c : B.l) c.fill (0.f);
      for (auto & c : B.r) c.fill (0.f);
      _wp = 0;
      for (auto & v : _lp_l) v = 0.f;
      for (auto & v : _lp_r) v = 0.f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      auto & B = *_buf;
      float ring_l = 0.f, ring_r = 0.f;
      for (int i = 0 ; i < NC ; ++i)
      {
         ring_l += comb (B.l[i], PL[i], _lp_l[i], in.left);
         ring_r += comb (B.r[i], PR[i], _lp_r[i], in.right);
      }
      if (++_wp >= N) _wp = 0;

      float g = _amount * 0.22f;
      return { in.left + g * ring_l, in.right + g * ring_r };
   }

private:
   static constexpr int N  = 512;
   static constexpr int NC = 3;

   struct Buffers { std::array<std::array<float,N>,NC> l{}, r{}; };

   static float   soft (float x)
   {
      if (x < -1.5f) return -1.f;
      if (x >  1.5f) return  1.f;
      return x - x * x * x * (1.f / 6.75f);
   }

   float          comb (std::array<float,N> & buf, int period, float & lp, float in)
   {
      int rp = _wp - period; if (rp < 0) rp += N;
      float dv = buf [rp];
      lp += 0.5f * (dv - lp);
      float y = soft (in + _fb * lp);    // bound comb Q so resonance never blows up
      buf [_wp] = y;
      return y - in;
   }

   erb::SdramPtr<Buffers> _buf;
   float          _amount = 0.f, _fb = 0.9f;
   float          _lp_l [NC] {}, _lp_r [NC] {};
   int            _wp = 0;

   static constexpr int PL [NC] = { 327, 163, 109 };
   static constexpr int PR [NC] = { 329, 164, 110 };
};

inline TailFxResonator::TailFxResonator (float sample_freq)
:  _buf (erb::make_sdram<Buffers> ())
{
   (void) sample_freq;
   reset ();
}

}  // namespace dsp
