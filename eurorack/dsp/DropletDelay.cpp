#include "DropletDelay.h"

#include <cmath>

namespace dsp
{

namespace
{

inline float rand01 (int & state)
{
   state = state * 1664525 + 1013904223;
   unsigned int x = static_cast <unsigned int> (state >> 1);
   return float (x & 0x7FFFFFFFu) / float (0x7FFFFFFFu);
}

} // anonymous namespace

DropletDelay::DropletDelay (float sample_freq, float max_delay_s)
:  _sample_freq (sample_freq)
,  _max_delay_s (max_delay_s)
{
   if (_max_delay_s <= 0.f)
   {
      _max_delay_s = 2.0f;
   }

   float max_delay_samples = _sample_freq * _max_delay_s;
   if (max_delay_samples < 1.f) max_delay_samples = 1.f;

   _buf_size = int (max_delay_samples) + 2;
   _buf_l.assign (_buf_size, 0.f);
   _buf_r.assign (_buf_size, 0.f);
   _write_pos = 0;
   _feedback  = 0.f;
   _amount    = 0.f;

   // Initialize taps with pseudo-random delays, pans and modulation speeds
   int   seed             = 123456789;
   float min_delay_s      = 0.03f;
   float max_delay_s_local= 0.90f;   // base delay range before feedback

   float max_delay_limit = float (_buf_size - 2);

   for (int i = 0 ; i < _nbr_taps ; ++i)
   {
      Tap & tap = _taps [i];

      float r_time  = rand01 (seed);
      float r_phase = rand01 (seed);
      float r_pan   = rand01 (seed);
      float r_speed = rand01 (seed);

      float t = (float (i) + 0.5f) / float (_nbr_taps); // 0..1

      float delay_s = min_delay_s + (max_delay_s_local - min_delay_s) * (0.3f + 0.7f * r_time) * t;
      float delay   = delay_s * _sample_freq;
      if (delay < 1.f) delay = 1.f;
      if (delay > max_delay_limit) delay = max_delay_limit;

      tap.base_delay = delay;
      tap.jitter     = 0.f;

      const float two_pi = 6.28318530717958647692f;
      tap.phase = r_phase * two_pi;

      // Slow modulation speeds for drifting delays: ~0.05..0.3 Hz
      float min_rate_hz = 0.05f;
      float max_rate_hz = 0.30f;
      float rate_hz = min_rate_hz + (max_rate_hz - min_rate_hz) * r_speed;
      tap.speed = two_pi * rate_hz / _sample_freq;

      // Random pan between L/R
      float pan   = r_pan * 2.f - 1.f; // -1..1
      float pan_l = 0.5f * (1.f - pan);
      float pan_r = 0.5f * (1.f + pan);
      tap.pan_l = pan_l;
      tap.pan_r = pan_r;

      // Nonlinear per-tap gain curve: emphasize mid taps with a Hann window
      float idx = (_nbr_taps > 1) ? (float (i) / float (_nbr_taps - 1)) : 0.f; // 0..1
      float window = 0.5f - 0.5f * std::cos (two_pi * idx); // 0..1
      float gain_shape = window / 0.5f;                      // ~0..2, mean ~1
      tap.gain = gain_shape / float (_nbr_taps);

      // Per-tap low-pass coefficient: early taps brighter, later darker
      float brightness = 1.f - idx; // early taps = higher brightness
      tap.lp_a = 0.25f + 0.45f * brightness; // 0.25 .. 0.70
      tap.lp_l = 0.f;
      tap.lp_r = 0.f;
   }
}

DropletDelay::~DropletDelay () = default;

void  DropletDelay::reset ()
{
   _write_pos = 0;
   _feedback  = 0.f;
   _amount    = 0.f;

   for (int i = 0 ; i < _buf_size ; ++i)
   {
      _buf_l [i] = 0.f;
      _buf_r [i] = 0.f;
   }

   // Reset only phases and filter state, keep structural parameters (delays, gains, pan)
   for (int i = 0 ; i < _nbr_taps ; ++i)
   {
      _taps [i].phase = 0.f;
      _taps [i].lp_l  = 0.f;
      _taps [i].lp_r  = 0.f;
   }
}

void  DropletDelay::set_amount (float amount)
{
   if (amount < 0.f) amount = 0.f;
   if (amount > 1.f) amount = 1.f;

   const float eps = 0.0001f;

   if (amount <= eps)
   {
      _amount   = 0.f;
      _feedback = 0.f;

      for (int i = 0 ; i < _nbr_taps ; ++i)
      {
         _taps [i].jitter = 0.f;
      }

      return;
   }

   _amount = amount;

   float k = amount;

   // Feedback: from gentle ambience to dense, stormy repeats
   // Slightly more aggressive to make the effect stand out
   _feedback = 0.25f + 0.60f * k;   // 0.25 .. 0.85

   // Jitter depth scales with amount: more motion at higher settings
   // Increased range for more pronounced "raindrop" movement
   float jitter_frac_min = 0.04f;
   float jitter_frac_max = 0.22f;
   float jitter_frac     = jitter_frac_min + (jitter_frac_max - jitter_frac_min) * k;

   for (int i = 0 ; i < _nbr_taps ; ++i)
   {
      Tap & tap = _taps [i];
      tap.jitter = jitter_frac * tap.base_delay;
   }
}

} // namespace dsp
