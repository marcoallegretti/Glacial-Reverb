/*****************************************************************************

     TailFx.h
     Post-model tail-effect interface. amount 0 leaves the tail untouched.

*Tab=3***********************************************************************/

#pragma once

#include "ReverbModel.h"

namespace dsp
{

class TailFx
{
public:
   virtual        ~TailFx () = default;
   virtual void   set_amount (float amount01) = 0;
   virtual void   reset () = 0;
   virtual StereoFrame process (StereoFrame in) = 0;
};

class TailFxNone : public TailFx
{
public:
   void           set_amount (float) override {}
   void           reset () override {}
   StereoFrame    process (StereoFrame in) override { return in; }
};

}  // namespace dsp
