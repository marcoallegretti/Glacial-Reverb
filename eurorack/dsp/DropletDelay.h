#pragma once

#include <vector>

namespace dsp
{

class DropletDelay
{
public:
   DropletDelay (float sample_freq, float max_delay_s = 2.0f);
   ~DropletDelay ();

   void  reset ();
   void  set_amount (float amount);   // one-knob macro 0..1
   void  process (float in_l, float in_r, float & out_l, float & out_r);

private:
   struct Tap
   {
      float base_delay = 0.f;  // base delay in samples
      float jitter     = 0.f;  // modulation depth in samples
      float phase      = 0.f;  // LFO phase [0..2pi]
      float speed      = 0.f;  // LFO increment in rad/sample
      float gain       = 0.f;  // per-tap gain
      float pan_l      = 0.5f; // pan gains sum to 1
      float pan_r      = 0.5f;
      float lp_l      = 0.f;  // per-tap lowpass state L
      float lp_r      = 0.f;  // per-tap lowpass state R
      float lp_a      = 0.f;  // per-tap lowpass coefficient
   };

   static constexpr int _nbr_taps = 24;

   float               _sample_freq = 0.f;
   float               _max_delay_s = 0.f;
   std::vector <float> _buf_l;
   std::vector <float> _buf_r;
   int                 _buf_size   = 0;
   int                 _write_pos  = 0;
   float               _feedback   = 0.f;
   float               _amount     = 0.f;
   Tap                 _taps [_nbr_taps];
};

} // namespace dsp

#include "DropletDelay.hpp"
