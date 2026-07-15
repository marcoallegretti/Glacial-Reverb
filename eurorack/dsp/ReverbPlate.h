/*****************************************************************************

     ReverbPlate.h
     Plate: Dattorro figure-8 tank.

*Tab=3***********************************************************************/

#pragma once

#include "ReverbModel.h"
#include "erb/SdramPtr.h"

#include <array>
#include <cmath>

namespace dsp
{

class ReverbPlate : public ReverbModel
{
public:
   explicit       ReverbPlate (float sample_freq);

   void           set_decay (float d) override;
   void           set_macro (float fx) override;
   void           set_low_pass_freq (float f) override;
   void           reset () override;
   StereoFrame    process (StereoFrame in) override;

private:
   // Buffer capacities at 48 kHz (Dattorro lengths x 1.613 + margin).
   static constexpr int S_IN1=256, S_IN2=192, S_IN3=640, S_IN4=512;
   static constexpr int S_LAP1=1152, S_LD1=7296, S_LAP2=3072, S_LD2=6144;
   static constexpr int S_RAP1=1536, S_RD1=6912, S_RAP2=4352, S_RD2=5248;

   struct Buffers
   {
      std::array<float,S_IN1> in1{}; std::array<float,S_IN2> in2{};
      std::array<float,S_IN3> in3{}; std::array<float,S_IN4> in4{};
      std::array<float,S_LAP1> lap1{}; std::array<float,S_LD1> ld1{};
      std::array<float,S_LAP2> lap2{}; std::array<float,S_LD2> ld2{};
      std::array<float,S_RAP1> rap1{}; std::array<float,S_RD1> rd1{};
      std::array<float,S_RAP2> rap2{}; std::array<float,S_RD2> rd2{};
   };

   struct Line
   {
      float * b = nullptr; int n = 0; int wp = 0;
      void  clear () { for (int i=0;i<n;++i) b[i]=0.f; wp=0; }
      void  write (float x) { b[wp]=x; if (++wp>=n) wp=0; }
      float read (int d) const { int p=wp-d; while (p<0) p+=n; return b[p]; }
      float readf (float d) const { int di=int(d); float f=d-float(di); float a=read(di), c=read(di+1); return a+(c-a)*f; }
   };

   static float   allpass (Line & l, float x, int len, float g)
   {
      float wD = l.read (len);
      float w  = x + g * wD;
      l.write (w);
      return wD - g * w;
   }
   static float   allpass_mod (Line & l, float x, float len, float g)
   {
      float wD = l.readf (len);
      float w  = x + g * wD;
      l.write (w);
      return wD - g * w;
   }

   float          _sr;
   erb::SdramPtr<Buffers> _buf;
   Line           _in1,_in2,_in3,_in4,_lap1,_ld1,_lap2,_ld2,_rap1,_rd1,_rap2,_rd2;
   int            _l_lap1=0,_l_ld1=0,_l_lap2=0,_l_ld2=0,_l_rap1=0,_l_rd1=0,_l_rap2=0,_l_rd2=0;
   int            _l_in1=0,_l_in2=0,_l_in3=0,_l_in4=0;

   float          _decay = 0.6f;
   float          _damp_c = 0.5f, _damp_l = 0.f, _damp_r = 0.f;
   float          _bw = 0.f, _bw_c = 0.6f;
   float          _cross_l = 0.f, _cross_r = 0.f;
   float          _lfo1 = 0.f, _lfo2 = 2.1f, _lfo1_inc = 0.f, _lfo2_inc = 0.f, _exc = 0.f;
};

inline ReverbPlate::ReverbPlate (float sample_freq)
:  _sr (sample_freq)
,  _buf (erb::make_sdram<Buffers> ())
{
   auto & B = *_buf;
   _in1.b=B.in1.data(); _in1.n=S_IN1; _in2.b=B.in2.data(); _in2.n=S_IN2;
   _in3.b=B.in3.data(); _in3.n=S_IN3; _in4.b=B.in4.data(); _in4.n=S_IN4;
   _lap1.b=B.lap1.data(); _lap1.n=S_LAP1; _ld1.b=B.ld1.data(); _ld1.n=S_LD1;
   _lap2.b=B.lap2.data(); _lap2.n=S_LAP2; _ld2.b=B.ld2.data(); _ld2.n=S_LD2;
   _rap1.b=B.rap1.data(); _rap1.n=S_RAP1; _rd1.b=B.rd1.data(); _rd1.n=S_RD1;
   _rap2.b=B.rap2.data(); _rap2.n=S_RAP2; _rd2.b=B.rd2.data(); _rd2.n=S_RD2;

   const float sc = _sr / 29761.f;
   auto L = [&] (int base, int cap) { int v=int(base*sc+0.5f); if (v>cap-3) v=cap-3; if (v<1) v=1; return v; };
   _l_in1=L(142,S_IN1); _l_in2=L(107,S_IN2); _l_in3=L(379,S_IN3); _l_in4=L(277,S_IN4);
   _l_lap1=L(672,S_LAP1); _l_ld1=L(4453,S_LD1); _l_lap2=L(1800,S_LAP2); _l_ld2=L(3720,S_LD2);
   _l_rap1=L(908,S_RAP1); _l_rd1=L(4217,S_RD1); _l_rap2=L(2656,S_RAP2); _l_rd2=L(3163,S_RD2);

   _exc = 12.f * sc;
   _lfo1_inc = 6.2831853f * 0.90f / _sr;
   _lfo2_inc = 6.2831853f * 0.71f / _sr;
   reset ();
}

inline void ReverbPlate::set_decay (float d)
{
   d = clamp01 (d);
   _decay = 0.4f + d * 0.5f;
}

inline void ReverbPlate::set_macro (float fx)
{
   fx = clamp01 (fx);
   float fc = 1500.f + fx * 10000.f;
   _damp_c = 1.f - std::exp (-6.2831853f * fc / _sr);
}

inline void ReverbPlate::set_low_pass_freq (float f)
{
   _bw_c = 1.f - std::exp (-6.2831853f * f / _sr);
}

inline void ReverbPlate::reset ()
{
   _in1.clear();_in2.clear();_in3.clear();_in4.clear();
   _lap1.clear();_ld1.clear();_lap2.clear();_ld2.clear();
   _rap1.clear();_rd1.clear();_rap2.clear();_rd2.clear();
   _damp_l=_damp_r=_bw=_cross_l=_cross_r=0.f; _lfo1=0.f; _lfo2=2.1f;
}

inline StereoFrame ReverbPlate::process (StereoFrame in)
{
   float x = 0.5f * (in.left + in.right);
   _bw += 0.75f * (x - _bw);
   x = _bw;

   x = allpass (_in1, x, _l_in1, 0.75f);
   x = allpass (_in2, x, _l_in2, 0.75f);
   x = allpass (_in3, x, _l_in3, 0.625f);
   x = allpass (_in4, x, _l_in4, 0.625f);

   _lfo1 += _lfo1_inc; if (_lfo1 > 6.2831853f) _lfo1 -= 6.2831853f;
   _lfo2 += _lfo2_inc; if (_lfo2 > 6.2831853f) _lfo2 -= 6.2831853f;

   float lin = x + _cross_r;
   float a = allpass_mod (_lap1, lin, float(_l_lap1) + _exc * std::sin (_lfo1), 0.70f);
   float la = _ld1.read (_l_ld1); _ld1.write (a);
   _damp_l += _damp_c * (la - _damp_l);
   a = _damp_l * _decay;
   a = allpass (_lap2, a, _l_lap2, 0.50f);
   float lout = _ld2.read (_l_ld2); _ld2.write (a);

   float rin = x + _cross_l;
   float b = allpass_mod (_rap1, rin, float(_l_rap1) + _exc * std::sin (_lfo2), 0.70f);
   float rb = _rd1.read (_l_rd1); _rd1.write (b);
   _damp_r += _damp_c * (rb - _damp_r);
   b = _damp_r * _decay;
   b = allpass (_rap2, b, _l_rap2, 0.50f);
   float rout = _rd2.read (_l_rd2); _rd2.write (b);

   _cross_l = lout * _decay;
   _cross_r = rout * _decay;

   float yl = _ld1.read (int(0.19f*_l_ld1)) + _ld1.read (int(0.66f*_l_ld1))
            - _ld2.read (int(0.42f*_l_ld2)) + _rd1.read (int(0.29f*_l_rd1))
            - _rd2.read (int(0.55f*_l_rd2)) + _rd2.read (int(0.13f*_l_rd2));
   float yr = _rd1.read (int(0.24f*_l_rd1)) + _rd1.read (int(0.61f*_l_rd1))
            - _rd2.read (int(0.31f*_l_rd2)) + _ld1.read (int(0.50f*_l_ld1))
            - _ld2.read (int(0.17f*_l_ld2)) + _ld2.read (int(0.71f*_l_ld2));

   return { yl * 0.6f, yr * 0.6f };
}

}  // namespace dsp
