/*****************************************************************************

     ReverbSpring.h
     Spring: parallel feedback delays with stretched-allpass dispersion.

*Tab=3***********************************************************************/

#pragma once

#include "ReverbModel.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class ReverbSpring : public ReverbModel
{
public:
   explicit       ReverbSpring (float sample_freq);

   void           set_decay (float d) override { _gain = 0.6f + clamp01 (d) * 0.38f; }
   void           set_macro (float fx) override { _disp = 0.5f + clamp01 (fx) * 0.35f; }
   void           set_low_pass_freq (float f) override
   {
      _damp = std::exp (-6.2831853f * f / _sr);
      if (_damp < 0.f) _damp = 0.f;
      if (_damp > 0.92f) _damp = 0.92f;
   }

   void           reset () override
   {
      auto & B = *_buf;
      B.s0.main.fill (0.f); B.s1.main.fill (0.f);
      for (auto & a : B.s0.ap) a.fill (0.f);
      for (auto & a : B.s1.ap) a.fill (0.f);
      _wp = 0; _lp0 = _lp1 = 0.f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      auto & B = *_buf;
      float x = 0.5f * (in.left + in.right);

      float o0 = spring (B.s0, LM0, KA0, _lp0, x);
      float o1 = spring (B.s1, LM1, KA1, _lp1, x);

      ++_wp;

      return { o0, o1 };
   }

private:
   static constexpr unsigned MAIN = 2048, MMASK = 2047;
   static constexpr unsigned AP = 512, AMASK = 511;
   static constexpr int NAP = 4;

   struct Spring
   {
      std::array<float,MAIN>              main{};
      std::array<std::array<float,AP>,NAP> ap{};
   };
   struct Buffers { Spring s0, s1; };

   static float   soft (float x)
   {
      if (x < -1.5f) return -1.f;
      if (x >  1.5f) return  1.f;
      return x - x * x * x * (1.f / 6.75f);
   }

   float          stretched_ap (std::array<float,AP> & buf, int k, float x) const
   {
      float vK = buf [(_wp - unsigned (k)) & AMASK];
      float v = x + _disp * vK;
      buf [_wp & AMASK] = v;
      return -_disp * v + vK;
   }

   float          spring (Spring & s, int lm, const int (&ka)[NAP], float & lp, float x)
   {
      float delayed = s.main [(_wp - unsigned (lm)) & MMASK];

      float fb = delayed;
      for (int i = 0 ; i < NAP ; ++i) fb = stretched_ap (s.ap[i], ka[i], fb);

      lp += (1.f - _damp) * (fb - lp);
      s.main [_wp & MMASK] = soft (x + _gain * lp);

      return delayed;
   }

   float          _sr;
   erb::SdramPtr<Buffers> _buf;
   float          _gain = 0.85f, _disp = 0.65f, _damp = 0.4f;
   float          _lp0 = 0.f, _lp1 = 0.f;
   unsigned       _wp = 0;

   static constexpr int LM0 = 1487, LM1 = 1993;
   static constexpr int KA0 [NAP] = { 113, 197, 251, 337 };
   static constexpr int KA1 [NAP] = { 149, 223, 293, 367 };
};

inline constexpr int ReverbSpring::KA0 [ReverbSpring::NAP];
inline constexpr int ReverbSpring::KA1 [ReverbSpring::NAP];

inline ReverbSpring::ReverbSpring (float sample_freq)
:  _sr (sample_freq)
,  _buf (erb::make_sdram<Buffers> ())
{
   set_low_pass_freq (4500.f);
   reset ();
}

}  // namespace dsp
