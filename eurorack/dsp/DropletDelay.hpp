#pragma once

#include <cmath>

namespace dsp
{

inline void  DropletDelay::process (float in_l, float in_r, float & out_l, float & out_r)
{
   float wet_l = in_l;
   float wet_r = in_r;

   if (_buf_size <= 2 || _amount <= 0.f)
   {
      // Still keep the buffer up to date to avoid transients when enabling
      if (_buf_size > 0)
      {
         _buf_l [_write_pos] = wet_l;
         _buf_r [_write_pos] = wet_r;
         ++_write_pos;
         if (_write_pos >= _buf_size) _write_pos = 0;
      }

      out_l = wet_l;
      out_r = wet_r;
      return;
   }

   const int write_pos = _write_pos;

   // Inject current wet sample into the delay network
   _buf_l [write_pos] = wet_l;
   _buf_r [write_pos] = wet_r;

   float acc_l = 0.f;
   float acc_r = 0.f;

   const float two_pi   = 6.28318530717958647692f;
   float       max_delay = float (_buf_size - 2);

   for (int i = 0 ; i < _nbr_taps ; ++i)
   {
      Tap & tap = _taps [i];

      float phase = tap.phase + tap.speed;
      if (phase > two_pi) phase -= two_pi;
      tap.phase = phase;

      float mod = std::sin (phase);   // -1..1

      float delay = tap.base_delay + tap.jitter * mod;
      if (delay < 0.f) delay = 0.f;
      if (delay > max_delay) delay = max_delay;

      float read_pos = float (write_pos) - delay;
      while (read_pos < 0.f)              read_pos += float (_buf_size);
      while (read_pos >= float (_buf_size)) read_pos -= float (_buf_size);

      int   idx0 = int (read_pos);
      int   idx1 = idx0 + 1;
      if (idx1 >= _buf_size) idx1 = 0;
      float frac = read_pos - float (idx0);

      float dl0 = _buf_l [idx0];
      float dl1 = _buf_l [idx1];
      float dr0 = _buf_r [idx0];
      float dr1 = _buf_r [idx1];

      float dl = dl0 + (dl1 - dl0) * frac;
      float dr = dr0 + (dr1 - dr0) * frac;

      // Per-tap low-pass filter: progressively darker taps along the chain
      float a = tap.lp_a;
      tap.lp_l += a * (dl - tap.lp_l);
      tap.lp_r += a * (dr - tap.lp_r);
      float fl = tap.lp_l;
      float fr = tap.lp_r;

      float g   = tap.gain;
      float out_tap_l = fl * g * tap.pan_l;
      float out_tap_r = fr * g * tap.pan_r;

      acc_l += out_tap_l;
      acc_r += out_tap_r;
   }

   // Scale aggregate droplet level as the macro increases so it stays present
   float intensity   = _amount;
   float effect_gain = 0.8f + 1.4f * intensity;   // 0.8 .. 2.2
   acc_l *= effect_gain;
   acc_r *= effect_gain;

   // Feed part of the raindrop sum back into the network for diffusion
   float fb = _feedback;
   _buf_l [write_pos] += acc_l * fb;
   _buf_r [write_pos] += acc_r * fb;

   ++_write_pos;
   if (_write_pos >= _buf_size) _write_pos = 0;

   // Mix original wet tail and droplet texture according to the macro amount.
   // Use a slightly front-loaded curve so the effect is audible even at mid settings.
   float mix = 0.2f + 0.8f * _amount;   // 0.2 .. 1.0
   out_l = wet_l * (1.f - mix) + acc_l * mix;
   out_r = wet_r * (1.f - mix) + acc_r * mix;
}

} // namespace dsp
