/*****************************************************************************

     ReverbModel.h
     Pluggable reverb model interface.

*Tab=3***********************************************************************/

#pragma once

namespace dsp
{

struct StereoFrame { float left; float right; };

inline float clamp01 (float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }

class ReverbModel
{
public:
   virtual        ~ReverbModel () = default;

   virtual void   set_decay (float decay01) = 0;
   virtual void   set_macro (float fx01) = 0;
   virtual void   set_low_pass_freq (float freq) = 0;
   virtual void   reset () = 0;
   virtual StereoFrame process (StereoFrame in) = 0;
};

}  // namespace dsp
