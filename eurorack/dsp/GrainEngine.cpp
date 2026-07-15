#include "GrainEngine.h"

#include <cmath>

namespace dsp
{

GrainEngine::GrainEngine (float sample_freq, float max_buffer_s)
:  _sample_freq (sample_freq)
,  _max_buffer_s (max_buffer_s)
{
	if (_max_buffer_s <= 0.f)
	{
		_max_buffer_s = 20.0f;
	}

	// Allocate a long circular buffer (default ~20 seconds, at least 2 seconds)
	_buf_size = int (_sample_freq * _max_buffer_s);
	if (_buf_size < int (_sample_freq * 2.f))
	{
		_buf_size = int (_sample_freq * 2.f);
	}

	_buf_l.assign (_buf_size, 0.f);
	_buf_r.assign (_buf_size, 0.f);

	// Envelope follower for gating grain layer to the reverb tail
	float attack_ms  = 5.f;
	float release_ms = 220.f;
	_env_attack  = 1.f - std::exp (-1.f / (_sample_freq * attack_ms / 1000.f));
	_env_release = 1.f - std::exp (-1.f / (_sample_freq * release_ms / 1000.f));
	_env = 0.f;

	_write_pos    = 0;
	_loop_len     = int (_sample_freq * 1.f);
	_filled_len   = 0;
	_amount       = 0.f;
	_grain_count  = 0;
	_grain_size_s = 0.1f;
	_delay_len_s  = 1.f;
	_spread       = 1.f;
	_pitch_a      = 1.f;
	_pitch_b      = 1.f;
	_window_type  = 0;
	_phase_a      = 0.f;
	_phase_b      = 0.f;
	_rand_state   = 1234567;

	for (int i = 0 ; i < _max_grains ; ++i)
	{
		_grains [i].start_a      = -1.f;
		_grains [i].start_b      = -1.f;
		_grains [i].offset_a     = 0.f;
		_grains [i].offset_b     = 0.f;
		_grains [i].prev_phase_a = 0.f;
		_grains [i].prev_phase_b = 0.f;
		_grains [i].pan_l        = 1.f;
		_grains [i].pan_r        = 1.f;
	}
}

GrainEngine::~GrainEngine () = default;

void  GrainEngine::reset ()
{
	_write_pos    = 0;
	_filled_len   = 0;
	_amount       = 0.f;
	_grain_count  = 0;
	_grain_size_s = 0.1f;
	_delay_len_s  = 1.f;
	_spread       = 1.f;
	_pitch_a      = 1.f;
	_pitch_b      = 1.f;
	_window_type  = 0;
	_phase_a      = 0.f;
	_phase_b      = 0.f;
	_rand_state   = 1234567;
	_loop_len     = int (_sample_freq * 1.f);
	_env          = 0.f;

	for (int i = 0 ; i < _buf_size ; ++i)
	{
		_buf_l [i] = 0.f;
		_buf_r [i] = 0.f;
	}

	for (int i = 0 ; i < _max_grains ; ++i)
	{
		_grains [i].start_a      = -1.f;
		_grains [i].start_b      = -1.f;
		_grains [i].offset_a     = 0.f;
		_grains [i].offset_b     = 0.f;
		_grains [i].prev_phase_a = 0.f;
		_grains [i].prev_phase_b = 0.f;
		_grains [i].pan_l        = 1.f;
		_grains [i].pan_r        = 1.f;
	}
}

void  GrainEngine::set_amount (float amount)
{
	if (amount < 0.f) amount = 0.f;
	if (amount > 1.f) amount = 1.f;
	_amount = amount;

	if (_buf_size <= 2)
	{
		_grain_count = 0;
		return;
	}

	float x = _amount;

	int grains = 0;
	if (x > 0.0001f)
	{
		// Increase density earlier so the effect is clearly audible before the top end.
		float d = std::sqrt (x);
		grains = 1 + int (d * float (_max_grains - 1) + 0.5f);
		if (grains < 1) grains = 1;
		if (grains > _max_grains) grains = _max_grains;
	}
	_grain_count = grains;

	// Keep grains in a musically useful range (avoid micro-grain "fan noise")
	float grain_size_s = 0.25f - 0.19f * x; // 250 ms -> ~60 ms
	if (grain_size_s < 0.04f) grain_size_s = 0.04f;
	if (grain_size_s > 0.30f) grain_size_s = 0.30f;
	_grain_size_s = grain_size_s;

	float delay_len_s = 0.20f + 0.90f * x; // 200 ms -> 1.1 s
	if (delay_len_s < 0.10f) delay_len_s = 0.10f;
	if (delay_len_s > 1.50f) delay_len_s = 1.50f;
	_delay_len_s = delay_len_s;

	_loop_len = int (_delay_len_s * _sample_freq);
	if (_loop_len < 2) _loop_len = 2;
	if (_loop_len > _buf_size) _loop_len = _buf_size;

	// Limit position jitter/spread so grains stay correlated (less "rain")
	_spread = 0.02f + 0.25f * x;
	if (_spread < 0.f)  _spread = 0.f;
	if (_spread > 0.35f) _spread = 0.35f;

	// Keep pitch near unity for pleasant texture; introduce small pitch spread only near the top.
	_pitch_a = 1.f;
	_pitch_b = 1.f;
	if (x > 0.90f)
	{
		float t = (x - 0.90f) / 0.10f;
		if (t < 0.f) t = 0.f;
		if (t > 1.f) t = 1.f;
		float d = 0.12f * t; // up to +/- 12% (avoid high-pitched artifacts)
		_pitch_a = 1.f - d;
		_pitch_b = 1.f + d;
	}

	// Prefer smoother windows by default
	if (x < 0.50f)      _window_type = 1;
	else                _window_type = 0;

	float grain_len = _grain_size_s * _sample_freq;
	if (grain_len < 2.f) grain_len = 2.f;
	for (int i = 0 ; i < _max_grains ; ++i)
	{
		// Offsets distribute grains across the grain phase (stable, avoids "rain" retrigger feel)
		float frac = (i + 0.5f) / float (_max_grains);
		float o_a = frac * grain_len;
		float o_b = (1.f - frac) * grain_len;
		_grains [i].offset_a = o_a;
		_grains [i].offset_b = o_b;
		_grains [i].prev_phase_a = 0.f;
		_grains [i].prev_phase_b = 0.f;

		_grains [i].start_a = -1.f;
		_grains [i].start_b = -1.f;

		// Stable stereo placement per grain, centered around 0.5 for active grains
		// so the image stays balanced regardless of grain count.
		float pan = 0.5f;
		if (_grain_count > 1)
		{
			// Spread active grains symmetrically: e.g. 2 grains -> 0.35, 0.65
			float spread = 0.35f; // how far from center (0 = mono, 0.5 = full L/R)
			float t = float (i) / float (_grain_count - 1); // 0..1 across active grains
			pan = 0.5f + spread * (t * 2.f - 1.f);          // center +/- spread
		}
		_grains [i].pan_l = 1.f - pan;
		_grains [i].pan_r = pan;
	}
}

void  GrainEngine::set_decay (float decay)
{
	if (decay < 0.f) decay = 0.f;
	if (decay > 1.f) decay = 1.f;
	_decay = decay;

	// Dynamically scale envelope release to follow the reverb decay time.
	// Short decay (~0) -> short release (~80ms), long decay (~1) -> long release (~800ms)
	float release_ms = 80.f + 720.f * _decay;
	_env_release = 1.f - std::exp (-1.f / (_sample_freq * release_ms / 1000.f));
}

} // namespace dsp
