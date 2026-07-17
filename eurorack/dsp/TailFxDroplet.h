/*****************************************************************************

     TailFxDroplet.h
     Droplet: scattered cross-fed echoes.

*Tab=3***********************************************************************/

#pragma once

#include "TailFx.h"
#include "erb/SdramPtr.h"

#include <array>

namespace dsp
{

class TailFxDroplet : public TailFx
{
public:
   explicit       TailFxDroplet (float sample_freq);

   void           set_amount (float a) override
   {
      _amount = clamp01 (a);
      _fb = 0.4f + _amount * 0.5f;
   }

   void           reset () override
   {
      auto & B = *_buf; B.l.fill (0.f); B.r.fill (0.f);
      _wp = 0; _lp_l = _lp_r = 0.f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      auto & B = *_buf;

      float tl = 0.f, tr = 0.f;
      for (int i = 0 ; i < NT ; ++i)
      {
         float s = read (B.l, TAP[i]);
         float t = read (B.r, TAP[i] + 37);
         tl += s * PAN_L[i];
         tr += t * PAN_R[i];
      }

      float fb_l = read (B.l, FBK);
      float fb_r = read (B.r, FBK);
      _lp_l += 0.4f * (fb_l - _lp_l);
      _lp_r += 0.4f * (fb_r - _lp_r);

      B.l [_wp] = soft (in.left  + _fb * _lp_r);    // cross-coupled feedback -> stereo scatter
      B.r [_wp] = soft (in.right + _fb * _lp_l);
      if (++_wp >= N) _wp = 0;

      float g = _amount * 0.5f;
      return { in.left + g * tl, in.right + g * tr };
   }

private:
   static constexpr int N  = 48000;
   static constexpr int NT = 5;
   static constexpr int FBK = 11003;

   struct Buffers { std::array<float,N> l{}, r{}; };

   static float   soft (float x)
   {
      if (x < -1.5f) return -1.f;
      if (x >  1.5f) return  1.f;
      return x - x * x * x * (1.f / 6.75f);
   }

   float          read (const std::array<float,N> & b, int delay) const
   {
      int rp = _wp - delay; while (rp < 0) rp += N;
      return b [rp];
   }

   erb::SdramPtr<Buffers> _buf;
   float          _amount = 0.f, _fb = 0.5f, _lp_l = 0.f, _lp_r = 0.f;
   int            _wp = 0;

   static constexpr int   TAP  [NT] = { 1873, 3547, 5981, 8221, 10513 };
   static constexpr float PAN_L[NT] = { 0.9f, 0.2f, 0.7f, 0.3f, 0.6f };
   static constexpr float PAN_R[NT] = { 0.2f, 0.9f, 0.4f, 0.8f, 0.5f };
};

inline TailFxDroplet::TailFxDroplet (float sample_freq)
:  _buf (erb::make_sdram<Buffers> ())
{
   (void) sample_freq;
   reset ();
}

}  // namespace dsp
