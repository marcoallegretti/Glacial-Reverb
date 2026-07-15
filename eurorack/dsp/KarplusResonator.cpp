#include "KarplusResonator.h"

#include <cmath>

namespace dsp
{

KarplusResonator::KarplusResonator (float sample_freq, float max_delay_s)
:  _sample_freq (sample_freq)
,  _max_delay_s (max_delay_s)
{
   if (_max_delay_s <= 0.f)
   {
      _max_delay_s = 0.1f;
   }

   _buf_size = int (_sample_freq * _max_delay_s) + 4;
   if (_buf_size < 64)
   {
      _buf_size = 64;
   }

   _buf_l.assign (_buf_size, 0.f);
   _buf_r.assign (_buf_size, 0.f);

   reset ();
}

KarplusResonator::~KarplusResonator () = default;

void  KarplusResonator::reset ()
{
   _write_pos     = 0;
   _delay_samples = 0.f;
   _feedback      = 0.f;
   _damp          = 0.f;
   _low_l         = 0.f;
   _low_r         = 0.f;
   _mix           = 0.f;

   for (int i = 0 ; i < _buf_size ; ++i)
   {
      _buf_l [i] = 0.f;
      _buf_r [i] = 0.f;
   }
}

void  KarplusResonator::set_amount (float amount)
{
   if (amount < 0.f) amount = 0.f;
   if (amount > 1.f) amount = 1.f;

   const float eps = 0.0001f;

   if (amount <= eps || _buf_size <= 2)
   {
      _mix           = 0.f;
      _feedback      = 0.f;
      _delay_samples = 0.f;
      return;
   }

   float f_min = 80.f;
   float f_max = 4000.f;
   float ratio = f_max / f_min;
   float f = f_min * std::pow (ratio, amount);

   float delay = _sample_freq / f;
   float max_delay = float (_buf_size - 2);
   if (delay < 2.f) delay = 2.f;
   if (delay > max_delay) delay = max_delay;
   _delay_samples = delay;

   float decay = 0.96f + 0.03f * amount;
   if (decay > 0.995f) decay = 0.995f;
   _feedback = decay;

   float bright = amount;
   _damp = 0.2f + (1.f - bright) * 0.6f;

   _mix = 0.3f + 0.7f * amount;
}

} // namespace dsp
