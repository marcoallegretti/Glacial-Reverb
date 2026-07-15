#pragma once

#include <vector>

namespace dsp
{

class KarplusResonator
{
public:
   KarplusResonator (float sample_freq, float max_delay_s = 0.1f);
   ~KarplusResonator ();

   void  reset ();
   void  set_amount (float amount);
   void  process (float in_l, float in_r, float & out_l, float & out_r);

private:
   float               _sample_freq = 0.f;
   float               _max_delay_s = 0.f;
   std::vector <float> _buf_l;
   std::vector <float> _buf_r;
   int                 _buf_size = 0;
   int                 _write_pos = 0;
   float               _delay_samples = 0.f;
   float               _feedback = 0.f;
   float               _damp = 0.f;
   float               _low_l = 0.f;
   float               _low_r = 0.f;
   float               _mix = 0.f;
};

} // namespace dsp

#include "KarplusResonator.hpp"
