/*****************************************************************************

     ci/dsp_smoke.cpp
     CI smoke test: build the reverb off-target and sweep every room x tail for
     NaN, Inf and gross overshoot. Fast enough for every push; deep soaks stay
     in the offline harness. Exit non-zero on the first failure.

*Tab=3***********************************************************************/

#include "KoloredVerbDsp.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

const float SR  = 48000.f;
const int   BLK = 48;

unsigned g_rs = 2166136261u;
float noise () { g_rs = g_rs * 1664525u + 1013904223u; return float (g_rs >> 8) / 8388608.f - 1.f; }

}  // namespace

int main ()
{
   int fails = 0;
   double worst = 0.0;

   for (int m = 0; m < KoloredVerbDsp::nbr_models; ++m)
   for (int t = 0; t < KoloredVerbDsp::nbr_tail_fx; ++t)
   {
      KoloredVerbDsp d (SR);
      d.set_fx_type (m);
      d.set_tail_fx (t);
      d.set_mix (1.f); d.set_decay (1.f); d.set_fx (1.f);
      d.set_tail_amount (1.f); d.set_pre_delay (0.3f); d.set_duck (0.f);
      d.set_frozen_level (1.f); d.set_frozen_mix (1.f); d.set_freeze (false);

      std::vector<float> il (BLK), ir (BLK), ol (BLK), orr (BLK);
      const float * in  [] = { il.data (), ir.data () };
      float * const out [] = { ol.data (), orr.data () };

      g_rs = 2166136261u;
      double pk = 0.0; bool bad = false;
      const int nblk = int (SR * 3) / BLK;      // 3 s per pair
      for (int b = 0; b < nblk; ++b)
      {
         for (int j = 0; j < BLK; ++j) { float s = 0.9f * noise (); il[j] = s; ir[j] = s; }
         d.process (out, in, BLK);
         for (int j = 0; j < BLK; ++j)
         {
            float y = ol[j], z = orr[j];
            if (std::isnan (y) || std::isinf (y) || std::isnan (z) || std::isinf (z)) bad = true;
            double a = std::fabs (double (y)); if (a > pk) pk = a;
         }
      }
      if (pk > worst) worst = pk;
      if (bad || pk > 2.0)
      {
         std::printf ("FAIL  model=%d tail=%d  peak=%.3f  nan=%d\n", m, t, pk, bad ? 1 : 0);
         ++fails;
      }
   }

   std::printf ("%d x %d pairs swept  worst peak %.4f  fails %d\n",
                KoloredVerbDsp::nbr_models, KoloredVerbDsp::nbr_tail_fx, worst, fails);
   return fails ? 1 : 0;
}
