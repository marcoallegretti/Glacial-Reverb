#pragma once

#include <vector>

namespace dsp
{

class GrainEngine
{
public:
   GrainEngine (float sample_freq, float max_buffer_s = 20.0f);
   ~GrainEngine ();

   void  reset ();
   void  set_amount (float amount);   // one-knob macro 0..1
   void  set_decay (float decay);     // reverb decay 0..1, used to scale envelope timing
   void  process (float in_l, float in_r, float & out_l, float & out_r);

private:
   struct Grain
   {
      float start_a     = -1.f;
      float start_b     = -1.f;
      float offset_a    = 0.f;
      float offset_b    = 0.f;
      float prev_phase_a = 0.f;
      float prev_phase_b = 0.f;
      float pan_l       = 0.5f;
      float pan_r       = 0.5f;
   };

   float               _sample_freq    = 0.f;
   float               _max_buffer_s   = 0.f;
   std::vector <float> _buf_l;
   std::vector <float> _buf_r;
   int                 _buf_size       = 0;
   int                 _write_pos      = 0;
   int                 _loop_len       = 0;
   int                 _filled_len     = 0;

   float               _env            = 0.f;
   float               _env_attack     = 0.f;
   float               _env_release    = 0.f;
   float               _decay          = 0.5f;

   float               _amount         = 0.f;
   int                 _grain_count    = 0;
   float               _grain_size_s   = 0.1f;
   float               _delay_len_s    = 1.f;
   float               _spread         = 1.f;
   float               _pitch_a        = 1.f;
   float               _pitch_b        = 1.f;
   int                 _window_type    = 0;

   float               _phase_a        = 0.f;
   float               _phase_b        = 0.f;

   static constexpr int _max_grains    = 10;
   Grain               _grains [_max_grains];

   int                 _rand_state     = 1;
};

} // namespace dsp

#include "GrainEngine.hpp"
