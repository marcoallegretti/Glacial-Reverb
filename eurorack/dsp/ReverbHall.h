/*****************************************************************************

     ReverbHall.h
     Hall: ReverbSc FDN tank.

*Tab=3***********************************************************************/

#pragma once

#include "ReverbModel.h"
#include "ReverbSc.h"

namespace dsp
{

class ReverbHall : public ReverbModel
{
public:
   explicit       ReverbHall (float sample_freq) : _rev (sample_freq) { _rev.set_low_pass_freq (16000.f); }

   void           set_decay (float d) override
   {
      d = clamp01 (d);
      _rev.set_feedback (0.3f + d * 0.65f);
   }

   void           set_macro (float fx) override
   {
      fx = clamp01 (fx);
      _rev.set_low_pass_freq (4000.f + fx * 15000.f);
   }

   void           set_low_pass_freq (float f) override { _rev.set_low_pass_freq (f); }
   void           reset () override { _rev.reset (); }

   StereoFrame    process (StereoFrame in) override
   {
      auto o = _rev.process ({ in.left, in.right });
      return { o.left, o.right };
   }

private:
   ReverbSc       _rev;
};

}  // namespace dsp
