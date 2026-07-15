#pragma once

#include <cmath>

namespace dsp
{

inline float rand01 (int & state)
{
	state = state * 1664525 + 1013904223;
	unsigned int x = static_cast <unsigned int> (state >> 1);
	return float (x & 0x7FFFFFFFu) / float (0x7FFFFFFFu);
}

inline void  GrainEngine::process (float in_l, float in_r, float & out_l, float & out_r)
{
	float wet_l = in_l;
	float wet_r = in_r;

	// Envelope follower to tie grain output to the reverb tail timing
	float in_abs = std::fabs (wet_l) + std::fabs (wet_r);
	if (in_abs > _env)
	{
		_env += _env_attack * (in_abs - _env);
	}
	else
	{
		_env += _env_release * (in_abs - _env);
	}

	if (_buf_size <= 2)
	{
		out_l = wet_l;
		out_r = wet_r;
		return;
	}

	// Use a looping delay line of length _loop_len (Faust: delayLength)
	int loop_len_i = _loop_len;
	if (loop_len_i < 2) loop_len_i = 2;
	if (loop_len_i > _buf_size) loop_len_i = _buf_size;

	if (_filled_len > loop_len_i) _filled_len = loop_len_i;

	// Write current input sample into the loop
	int write_pos = _write_pos;
	if (write_pos < 0) write_pos = 0;
	if (write_pos >= loop_len_i) write_pos %= loop_len_i;
	_buf_l [write_pos] = wet_l;
	_buf_r [write_pos] = wet_r;

	++_write_pos;
	if (_write_pos >= loop_len_i) _write_pos = 0;
	if (_filled_len < loop_len_i) ++_filled_len;

	int active_len_i = loop_len_i;
	if (_filled_len < active_len_i) active_len_i = _filled_len;
	if (active_len_i < 2)
	{
		out_l = wet_l;
		out_r = wet_r;
		return;
	}
	float loop_len_f = float (active_len_i);

	if (_grain_count <= 0 || _amount <= 0.f)
	{
		out_l = wet_l;
		out_r = wet_r;
		return;
	}

	float grain_len = _grain_size_s * _sample_freq;
	if (grain_len < 2.f) grain_len = 2.f;

	// Shorter base offsets reduce perceived latency and keep grains related to the current tail
	float base_offset_a = 0.10f * loop_len_f;
	float base_offset_b = 0.06f * loop_len_f;
	float jitter_scale  = _spread * loop_len_f * 0.10f;

	float step_a = _pitch_a;
	float step_b = _pitch_b;
	if (std::fabs (step_a) < 0.01f) step_a = (step_a < 0.f) ? -0.01f : 0.01f;
	if (std::fabs (step_b) < 0.01f) step_b = (step_b < 0.f) ? -0.01f : 0.01f;

	_phase_a += step_a;
	while (_phase_a >= grain_len) _phase_a -= grain_len;
	while (_phase_a < 0.f)        _phase_a += grain_len;
	_phase_b += step_b;
	while (_phase_b >= grain_len) _phase_b -= grain_len;
	while (_phase_b < 0.f)        _phase_b += grain_len;

	const float two_pi = 6.28318530717958647692f;
	float norm = 1.f;
	if (_grain_count > 0)
	{
		norm = 1.f / std::sqrt (float (_grain_count));
	}

	auto window = [&] (float ph) -> float
	{
		if (ph < 0.f) ph = 0.f;
		if (ph > 1.f) ph = 1.f;
		if (_window_type == 0)
		{
			return std::sin (ph * 3.14159265358979323846f);
		}
		if (_window_type == 1)
		{
			return 0.54f - 0.46f * std::cos (two_pi * ph);
		}
		return 1.f
			- 1.93f * std::cos (two_pi * ph)
			+ 1.29f * std::cos (2.f * two_pi * ph)
			- 0.388f * std::cos (3.f * two_pi * ph)
			+ 0.028f * std::cos (4.f * two_pi * ph);
	};

	auto wrap_loop = [&] (float pos) -> float
	{
		while (pos >= loop_len_f) pos -= loop_len_f;
		while (pos < 0.f)         pos += loop_len_f;
		return pos;
	};

	auto idx_wrap = [&] (int idx) -> int
	{
		while (idx < 0) idx += active_len_i;
		while (idx >= active_len_i) idx -= active_len_i;
		return idx;
	};

	auto interp_hermite = [&] (const std::vector <float> & buf, float pos) -> float
	{
		pos = wrap_loop (pos);
		int idx1 = int (pos);
		float x = pos - float (idx1);
		int idx0 = idx_wrap (idx1 - 1);
		int idx2 = idx_wrap (idx1 + 1);
		int idx3 = idx_wrap (idx1 + 2);
		idx1 = idx_wrap (idx1);

		float y0 = buf [idx0];
		float y1 = buf [idx1];
		float y2 = buf [idx2];
		float y3 = buf [idx3];

		float c0 = y1;
		float c1 = 0.5f * (y2 - y0);
		float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
		float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
		return ((c3 * x + c2) * x + c1) * x + c0;
	};

	float acc_l = 0.f;
	float acc_r = 0.f;

	for (int i = 0 ; i < _grain_count ; ++i)
	{
		Grain & g = _grains [i];

		float ca = _phase_a + g.offset_a;
		while (ca >= grain_len) ca -= grain_len;
		while (ca < 0.f)        ca += grain_len;
		float cb = _phase_b + g.offset_b;
		while (cb >= grain_len) cb -= grain_len;
		while (cb < 0.f)        cb += grain_len;

		bool wrap_a = (step_a >= 0.f) ? (ca < g.prev_phase_a) : (ca > g.prev_phase_a);
		bool wrap_b = (step_b >= 0.f) ? (cb < g.prev_phase_b) : (cb > g.prev_phase_b);
		g.prev_phase_a = ca;
		g.prev_phase_b = cb;

		if (wrap_a)
		{
			// Choose start positions relative to the write head (coherent texture)
			// Base offset: read from the recent past inside the loop.
			float jitter = (rand01 (_rand_state) * 2.f - 1.f) * jitter_scale;
			g.start_a = wrap_loop (float (write_pos) - base_offset_a + jitter);
		}
		if (wrap_b)
		{
			float jitter = (rand01 (_rand_state) * 2.f - 1.f) * jitter_scale;
			g.start_b = wrap_loop (float (write_pos) - base_offset_b + jitter);
		}

		// Initialize immediately when enabling the effect (prevents delayed onset)
		if (g.start_a < 0.f)
		{
			g.start_a = wrap_loop (float (write_pos) - base_offset_a);
			g.prev_phase_a = ca;
		}
		if (g.start_b < 0.f)
		{
			g.start_b = wrap_loop (float (write_pos) - base_offset_b);
			g.prev_phase_b = cb;
		}

		float pos_a = wrap_loop (g.start_a + ca);
		float pos_b = wrap_loop (g.start_b + cb);

		float la = interp_hermite (_buf_l, pos_a);
		float ra = interp_hermite (_buf_r, pos_a);
		float lb = interp_hermite (_buf_l, pos_b);
		float rb = interp_hermite (_buf_r, pos_b);

		float wa = window (ca / grain_len);
		float wb = window (cb / grain_len);

		float gl = (la * wa + lb * wb) * 0.5f;
		float gr = (ra * wa + rb * wb) * 0.5f;

		acc_l += gl * g.pan_l * norm;
		acc_r += gr * g.pan_r * norm;
	}

	// Additive layer: keep reverb tail present while adding grain cloud on top.
	// This avoids the "FX up -> reverb disappears" perception.
	float x = _amount;
	if (x < 0.f) x = 0.f;
	if (x > 1.f) x = 1.f;

	float grain_gain = 0.f;
	if (x > 0.03f)
	{
		float t = (x - 0.03f) / 0.97f;
		if (t < 0.f) t = 0.f;
		if (t > 1.f) t = 1.f;
		// Stronger gain curve so granulation is clearly audible
		float shaped = std::pow (t, 0.55f);
		grain_gain = 0.4f + 1.6f * shaped;
	}

	// Shape grain output by tail envelope (release is dynamically tied to reverb decay).
	// Use sqrt for a gentler fade that keeps grains audible longer during the tail.
	float env_shape = std::sqrt (_env * 4.f);
	if (env_shape > 1.f) env_shape = 1.f;
	grain_gain *= env_shape;

	out_l = wet_l + acc_l * grain_gain;
	out_r = wet_r + acc_r * grain_gain;
}

} // namespace dsp
