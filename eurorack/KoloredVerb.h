/*****************************************************************************

     KoloredVerb.h

*Tab=3***********************************************************************/

/*\\ INCLUDE FILES \\\\*/

#include "KoloredVerbDsp.h"
#include "artifacts/KoloredVerbUi.h"

#include "erb/erb.h"


/*\\ PUBLIC \\\\*/

struct KoloredVerb
{
   KoloredVerbDsp dsp { erb_SAMPLE_RATE };
   KoloredVerbUi  ui;

   bool  freeze_state = false;
   bool  tank_state   = true;

   // Two-tier selectors. Perfect-B layout: the RGB LED belongs to the ROOM
   // selector (fx_button, top-right), the red LED to the tail-FX selector
   // (tank_button, bottom-right). Both: short = family, long = variant.
   int   room_family  = 0;
   int   room_variant = 0;   // ROOM  -> fx_button + RGB fx_led (family colour)
   int   fx_family    = 0;
   int   fx_variant   = 0;   // TailFX -> tank_button + red tank_led (family count)

   // Button gestures (short/long), collision-free
   float freeze_hold_s   = 0.f;  bool freeze_long_fired = false;
   float room_hold_s     = 0.f;  bool room_long_fired   = false;
   float fx_hold_s       = 0.f;  bool fx_long_fired     = false;

   float led_s        = 0.f;   // free-running LED timer
   bool  fx_led_init  = false;

   void  process ();
};
