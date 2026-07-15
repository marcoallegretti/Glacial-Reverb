/*****************************************************************************

     Flanger.hpp
     Stereo flanger effect - inline process implementation

*Tab=3***********************************************************************/

#pragma once

#include <cmath>

namespace dsp
{

inline void  Flanger::process (float in_l, float in_r, float & out_l, float & out_r)
{
	// Bypass when amount is zero
	if (_amount <= 0.0001f)
	{
		out_l = in_l;
		out_r = in_r;
		return;
	}

	// LFO: triangle wave for smooth modulation
	auto triangle = [] (float phase) -> float
	{
		// phase in [0,1), output in [0,1]
		float t = phase * 2.f;
		if (t > 1.f) t = 2.f - t;
		return t;
	};

	// Advance LFO phases
	float lfo_inc = _rate / _sample_freq;
	_lfo_phase_l += lfo_inc;
	if (_lfo_phase_l >= 1.f) _lfo_phase_l -= 1.f;
	_lfo_phase_r += lfo_inc;
	if (_lfo_phase_r >= 1.f) _lfo_phase_r -= 1.f;

	// Compute modulated delay times
	float lfo_l = triangle (_lfo_phase_l);
	float lfo_r = triangle (_lfo_phase_r);

	float delay_l = _delay_min_s + _delay_range_s * lfo_l;
	float delay_r = _delay_min_s + _delay_range_s * lfo_r;

	// Convert to samples (fractional)
	float delay_samples_l = delay_l * _sample_freq;
	float delay_samples_r = delay_r * _sample_freq;

	// Clamp to buffer size
	float max_delay = float (_buf_size - 2);
	if (delay_samples_l > max_delay) delay_samples_l = max_delay;
	if (delay_samples_r > max_delay) delay_samples_r = max_delay;
	if (delay_samples_l < 1.f) delay_samples_l = 1.f;
	if (delay_samples_r < 1.f) delay_samples_r = 1.f;

	// Write input + feedback into delay buffer
	int wp = _write_pos;
	_buf_l [wp] = in_l - _fb_l * _feedback;
	_buf_r [wp] = in_r - _fb_r * _feedback;

	// Fractional delay read with linear interpolation
	auto fdelay = [&] (const std::vector <float> & buf, float delay) -> float
	{
		float read_pos = float (wp) - delay;
		if (read_pos < 0.f) read_pos += float (_buf_size);

		int idx0 = int (read_pos);
		int idx1 = idx0 + 1;
		if (idx0 >= _buf_size) idx0 -= _buf_size;
		if (idx1 >= _buf_size) idx1 -= _buf_size;
		if (idx0 < 0) idx0 += _buf_size;
		if (idx1 < 0) idx1 += _buf_size;

		float frac = read_pos - std::floor (read_pos);
		return buf [idx0] * (1.f - frac) + buf [idx1] * frac;
	};

	float delayed_l = fdelay (_buf_l, delay_samples_l);
	float delayed_r = fdelay (_buf_r, delay_samples_r);

	// Store for feedback next sample
	_fb_l = delayed_l;
	_fb_r = delayed_r;

	// Advance write position
	++_write_pos;
	if (_write_pos >= _buf_size) _write_pos = 0;

	// Mix: dry + wet (Faust-style: 0.5 * (dry + depth * delayed))
	// For flanger, we want the comb filtering effect from summing dry + delayed
	float wet_l = in_l + _depth * delayed_l;
	float wet_r = in_r + _depth * delayed_r;

	out_l = wet_l * 0.5f;
	out_r = wet_r * 0.5f;
}

} // namespace dsp
