/*****************************************************************************

     Flanger.cpp
     Stereo flanger effect based on Faust phaflangers.lib

*Tab=3***********************************************************************/

#include "Flanger.h"

#include <cmath>
#include <algorithm>

namespace dsp
{

Flanger::Flanger (float sample_freq)
:  _sample_freq (sample_freq)
{
	// Max delay ~15 ms (power of 2 for efficiency)
	_buf_size = 1;
	while (_buf_size < int (_sample_freq * 0.015f))
	{
		_buf_size *= 2;
	}

	_buf_l.assign (_buf_size, 0.f);
	_buf_r.assign (_buf_size, 0.f);

	_write_pos     = 0;
	_amount        = 0.f;
	_depth         = 0.f;
	_feedback      = 0.f;
	_rate          = 0.5f;
	_delay_min_s   = 0.001f;  // 1 ms
	_delay_range_s = 0.007f;  // 7 ms sweep

	// Stereo LFO: right channel offset by 0.25 (90 degrees) for width
	_lfo_phase_l   = 0.f;
	_lfo_phase_r   = 0.25f;

	_fb_l          = 0.f;
	_fb_r          = 0.f;
}

Flanger::~Flanger () = default;

void  Flanger::reset ()
{
	_write_pos   = 0;
	_lfo_phase_l = 0.f;
	_lfo_phase_r = 0.25f;
	_fb_l        = 0.f;
	_fb_r        = 0.f;

	for (int i = 0 ; i < _buf_size ; ++i)
	{
		_buf_l [i] = 0.f;
		_buf_r [i] = 0.f;
	}
}

void  Flanger::set_amount (float amount)
{
	if (amount < 0.f) amount = 0.f;
	if (amount > 1.f) amount = 1.f;
	_amount = amount;

	// Map macro to flanger parameters for musical results:
	// - Depth: 0 -> 0, 1 -> 1 (linear)
	// - Feedback: 0 -> 0, 1 -> 0.7 (moderate feedback, avoids runaway)
	// - Rate: 0 -> 0.1 Hz, 1 -> 2.5 Hz (slow to medium sweep)
	// - Delay range: 0 -> 2ms, 1 -> 8ms

	_depth         = amount;
	_feedback      = amount * 0.7f;
	_rate          = 0.1f + amount * 2.4f;
	_delay_min_s   = 0.0005f + amount * 0.0015f;  // 0.5 - 2 ms
	_delay_range_s = 0.002f + amount * 0.006f;    // 2 - 8 ms
}

} // namespace dsp
