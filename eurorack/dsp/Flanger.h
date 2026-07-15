/*****************************************************************************

     Flanger.h
     Stereo flanger effect based on Faust phaflangers.lib

*Tab=3***********************************************************************/

#pragma once

#include <vector>

namespace dsp
{

class Flanger
{
public:
   Flanger (float sample_freq);
   ~Flanger ();

   void  reset ();
   void  set_amount (float amount);   // one-knob macro 0..1
   void  process (float in_l, float in_r, float & out_l, float & out_r);

private:
   float               _sample_freq   = 0.f;

   // Delay lines (stereo)
   std::vector <float> _buf_l;
   std::vector <float> _buf_r;
   int                 _buf_size      = 0;
   int                 _write_pos     = 0;

   // Parameters (derived from macro)
   float               _amount        = 0.f;
   float               _depth         = 0.f;   // effect strength 0..1
   float               _feedback      = 0.f;   // feedback 0..~0.9
   float               _rate          = 0.f;   // LFO rate in Hz
   float               _delay_min_s   = 0.f;   // minimum delay in seconds
   float               _delay_range_s = 0.f;   // delay sweep range in seconds

   // LFO state (two phases for stereo)
   float               _lfo_phase_l   = 0.f;
   float               _lfo_phase_r   = 0.f;

   // Feedback state
   float               _fb_l          = 0.f;
   float               _fb_r          = 0.f;
};

} // namespace dsp

#include "Flanger.hpp"
