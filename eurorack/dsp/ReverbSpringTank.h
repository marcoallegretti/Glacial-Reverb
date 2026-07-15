/*****************************************************************************

     ReverbSpringTank.h
     Spring (long tank): 3 coupled channels, counter-wound pairs, spread delays.

*Tab=3***********************************************************************/

#pragma once

#include "ReverbModel.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class ReverbSpringTank : public ReverbModel
{
public:
   explicit       ReverbSpringTank (float sample_freq);

   // long tanks are specified at 2.75-4.0 s, short ones at 1.2-2.0 s
   void           set_decay (float d) override { _gain = 0.770f + clamp01 (d) * 0.158f; }

   // macro = spring gauge: chirp rate depends on wire/helix radius, not on tank
   // length, so this moves the boing without moving the echo spacing
   void           set_macro (float fx) override { _disp = 0.42f + clamp01 (fx) * 0.34f; }

   void           set_low_pass_freq (float f) override
   {
      _damp = std::exp (-6.2831853f * f / _sr);
      if (_damp < 0.f) _damp = 0.f;
      if (_damp > 0.9f) _damp = 0.9f;
   }

   void           reset () override
   {
      auto & B = *_buf;
      for (auto & m : B.h1) m.fill (0.f);
      for (auto & m : B.h2) m.fill (0.f);
      for (auto & c : B.ap) for (auto & a : c) a.fill (0.f);
      _wp = 0;
      for (int i = 0 ; i < NCH ; ++i) { _lp[i] = 0.f; _hp[i] = 0.f; _hx[i] = 0.f; _fb[i] = 0.f; }
      _lfo = 0.f;
   }

   StereoFrame    process (StereoFrame in) override
   {
      auto & B = *_buf;
      float x = 0.5f * (in.left + in.right);

      float mod = 2.0f * std::sin (_lfo);
      _lfo += _lfo_inc; if (_lfo >= 6.2831853f) _lfo -= 6.2831853f;

      float o [NCH];
      for (int i = 0 ; i < NCH ; ++i)
      {
         // the two halves are one counter-wound pair, so the round trip is the
         // whole tank; the ring between them scatters instead of just passing
         float mid = read (B.h1[i], _half[i] + mod * (i == 1 ? -1.f : 1.f));
         float end = read (B.h2[i], _half[i]);

         float thru = mid * (1.f - RING);
         float refl = mid * RING;

         float v = chirp (B.ap[i], _k[i], end);

         _lp[i] += (1.f - _damp) * (v - _lp[i]);
         float h = _lp[i] - _hx[i] + 0.9975f * _hp[i]; _hx[i] = _lp[i]; _hp[i] = h;

         // negative: the first echo off a real tank comes back flipped
         _fb[i] = soft (G_FB * _gain * h);

         B.h1[i][_wp & HMASK] = soft (x * IN_G + _fb[i] + refl * 0.5f);
         B.h2[i][_wp & HMASK] = soft (thru);
         o[i] = h;
      }
      ++_wp;

      // the springs share one mounting plate, so the channels bleed into each
      // other rather than staying independent
      float l = o[0] * 0.62f + o[2] * 0.38f;
      float r = o[1] * 0.62f + o[2] * 0.38f;
      return { l * NORM, r * NORM };
   }

private:
   static constexpr int      NCH = 3;
   static constexpr unsigned H = 2048, HMASK = 2047;
   static constexpr unsigned AP = 128, AMASK = 127;
   static constexpr int      NAP = 6;
   static constexpr float    NORM = 2.3f;
   static constexpr float    IN_G = 0.6f;
   static constexpr float    G_FB = -1.f;     // measured tanks reverse echo 1
   static constexpr float    RING = 0.28f;    // mid-tank coupling reflection

   // Long tank (16.75"): delays spread 39-51 ms while the chirp cutoffs stay
   // clustered -- the inverse of a short tank, which clusters delays and
   // spreads cutoffs. That contrast is what makes this a different voice.
   static constexpr float TD_MS [NCH] = { 39.2f, 40.5f, 51.3f };
   static constexpr int   K [NCH]     = { 22, 24, 23 };   // ~4.6-5.0 kHz cutoffs

   struct Buffers
   {
      std::array<std::array<float,H>,NCH>                        h1{}, h2{};
      std::array<std::array<std::array<float,AP>,NAP>,NCH>       ap{};
   };

   static float   soft (float x)
   {
      if (x < -1.5f) return -1.f;
      if (x >  1.5f) return  1.f;
      return x - x * x * x * (1.f / 6.75f);
   }

   float          read (const std::array<float,H> & b, float delay) const
   {
      float rp = float (_wp & HMASK) - delay;
      while (rp < 0.f) rp += float (H);
      int i1 = int (rp) & int (HMASK); float f = rp - std::floor (rp);
      int i0 = (i1 - 1) & int (HMASK), i2 = (i1 + 1) & int (HMASK), i3 = (i1 + 2) & int (HMASK);
      float y0 = b[i0], y1 = b[i1], y2 = b[i2], y3 = b[i3];
      float c1 = 0.5f*(y2-y0), c2 = y0 - 2.5f*y1 + 2.f*y2 - 0.5f*y3, c3 = 0.5f*(y3-y0)+1.5f*(y1-y2);
      return ((c3*f + c2)*f + c1)*f + y1;
   }

   float          chirp (std::array<std::array<float,AP>,NAP> & ap, int k, float x)
   {
      for (int m = 0 ; m < NAP ; ++m)
      {
         auto & buf = ap[m];
         float wK = buf [(_wp - unsigned (k)) & AMASK];
         float w = x - _disp * wK;
         buf [_wp & AMASK] = w;
         x = _disp * w + wK;
      }
      return x;
   }

   float          _sr;
   erb::SdramPtr<Buffers> _buf;
   float          _half [NCH] {};
   int            _k [NCH] {};
   float          _gain = 0.85f, _disp = 0.6f, _damp = 0.45f;
   float          _lp [NCH] {}, _hp [NCH] {}, _hx [NCH] {}, _fb [NCH] {};
   float          _lfo = 0.f, _lfo_inc = 0.f;
   unsigned       _wp = 0;
};

inline ReverbSpringTank::ReverbSpringTank (float sample_freq)
:  _sr (sample_freq)
,  _buf (erb::make_sdram<Buffers> ())
{
   for (int i = 0 ; i < NCH ; ++i)
   {
      float td = TD_MS[i] * _sr * 0.001f;
      _half[i] = 0.5f * td;                       // each half of a counter-wound pair
      float lim = float (H - 4);
      if (_half[i] > lim) _half[i] = lim;
      _k[i] = K[i];
   }
   _lfo_inc = 6.2831853f * 0.45f / _sr;
   set_low_pass_freq (5000.f);                    // tanks give almost nothing above 6-7 kHz
   reset ();
}

}  // namespace dsp
