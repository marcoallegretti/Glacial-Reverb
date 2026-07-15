/*****************************************************************************

     GlacialReverbDsp.cpp
     Reverb wrapper: freeze pad, duck, mix and pre-delay around the active model.

*Tab=3***********************************************************************/

/*\\ INCLUDE FILES \\\\*/

#include "GlacialReverbDsp.h"

#include <cstddef>
#include <cmath>


namespace {

// Bounds the wet path so no model x tail x amount can clip. Thresholded: the
// old curve was a static cubic with no threshold, so it distorted every model at
// every level (~2% THD on an ordinary tail) and left no clean voice available.
// Below 0.9 this is bit-transparent; 0.9..1.1 is a C1 knee (unit slope at 0.9,
// zero at 1.1) that reaches the 1.0 ceiling and hard-bounds beyond it.
inline float soft_wet (float x)
{
   const float t = 0.9f;
   float a = std::fabs (x);
   if (a <= t) return x;
   if (a >= 1.1f) return (x < 0.f) ? -1.f : 1.f;
   float u = a - t;
   float y = t + u - u * u * 2.5f;
   return (x < 0.f) ? -y : y;
}

}  // namespace


/*\\ PUBLIC \\\\*/

GlacialReverbDsp::GlacialReverbDsp (float sample_freq)
:  _sample_freq (sample_freq)
,  _hall (sample_freq)
,  _plate (sample_freq)
,  _shimmer (sample_freq)
,  _cloud (sample_freq)
,  _ambient (sample_freq)
,  _spring (sample_freq)
,  _abyss (sample_freq)
,  _spring_modal (sample_freq)
,  _spring_disp (sample_freq)
,  _plate_modal (sample_freq)
,  _plate_disp (sample_freq)
,  _chamber (sample_freq)
,  _tail_reso (sample_freq)
,  _tail_pitch (sample_freq)
,  _tail_chorus (sample_freq)
,  _tail_granular (sample_freq)
,  _tail_droplet (sample_freq)
,  _tail_sub (sample_freq)
,  _tail_reverse (sample_freq)
,  _tail_chord (sample_freq)
,  _tail_formant (sample_freq)
,  _reverb_frozen (sample_freq)
{
   _wet.target = 0.f;
   _frozen_input_gain.target = 1.f;
   _tank_inject_gain.target  = 1.f;

   _active_model = &_hall;
   _active_tail  = &_tail_none;

   _reverb_frozen.set_low_pass_freq (18000.f);

   _pre_delay_max_spl = _sample_freq * 0.250f;
   _pre_delay_buf_size = int (_pre_delay_max_spl) + 4;
   if (_pre_delay_buf_size < 512) _pre_delay_buf_size = 512;
   _pre_delay_buf_l = new float [_pre_delay_buf_size];
   _pre_delay_buf_r = new float [_pre_delay_buf_size];
   for (int i = 0 ; i < _pre_delay_buf_size ; ++i)
   {
      _pre_delay_buf_l [i] = 0.f;
      _pre_delay_buf_r [i] = 0.f;
   }
   _pre_delay_write_pos = 0;
   _pre_delay_spl = 1.f;

   _duck_atk = 1.f - std::exp (-1.f / (_sample_freq * 0.005f));
   _duck_rel = 1.f - std::exp (-1.f / (_sample_freq * 0.250f));

   // the ramp has to outlast the incoming model's build-up, or the outgoing tail
   // is retired before there is anything to hand over to and the wet still dips
   _xfade_inc = 1.f / (_sample_freq * 0.150f);
}

GlacialReverbDsp::~GlacialReverbDsp ()
{
   delete [] _pre_delay_buf_l;
   delete [] _pre_delay_buf_r;
}

void  GlacialReverbDsp::set_mix (float mix)
{
   if (mix < 0.f) mix = 0.f;
   if (mix > 1.f) mix = 1.f;
   _mix = mix;
}

void  GlacialReverbDsp::set_decay (float decay)
{
   if (decay < 0.f) decay = 0.f;
   if (decay > 1.f) decay = 1.f;
   _decay = decay;
   if (_active_model) _active_model->set_decay (decay);
}

void  GlacialReverbDsp::set_pre_delay (float pre_delay)
{
   if (pre_delay < 0.f) pre_delay = 0.f;
   if (pre_delay > 1.f) pre_delay = 1.f;
   _pre_delay = pre_delay;
}

void  GlacialReverbDsp::set_frozen_level (float level)
{
   if (level < 0.f) level = 0.f;
   if (level > 1.f) level = 1.f;
   _frozen_level = level;
}

void  GlacialReverbDsp::set_frozen_mix (float mix)
{
   if (mix < 0.f) mix = 0.f;
   if (mix > 1.f) mix = 1.f;
   _frozen_mix = mix;
}

void  GlacialReverbDsp::set_freeze (bool freeze)
{
   if (freeze != _freeze)
   {
      _freeze_hold_s = 0.f;
   }
   _freeze = freeze;
}

void  GlacialReverbDsp::set_fx (float fx)
{
   if (fx < 0.f) fx = 0.f;
   if (fx > 1.f) fx = 1.f;
   _fx_param = fx;
   if (_active_model) _active_model->set_macro (fx);
}

void  GlacialReverbDsp::set_tank (bool enabled)
{
   _tank_enabled = enabled;
}

dsp::ReverbModel *   GlacialReverbDsp::model_for_id (int id)
{
   switch (id)
   {
   case 1:  return &_plate;
   case 2:  return &_shimmer;
   case 3:  return &_cloud;
   case 4:  return &_ambient;
   case 5:  return &_spring;
   case 6:  return &_abyss;
   case 7:  return &_spring_modal;
   case 8:  return &_spring_disp;
   case 9:  return &_plate_modal;
   case 10: return &_plate_disp;
   case 11: return &_chamber;
   default: return &_hall;
   }
}

void  GlacialReverbDsp::set_fx_type (int fx_index)
{
   if (fx_index < 0) fx_index = 0;
   if (fx_index >= nbr_models) fx_index = nbr_models - 1;
   if (fx_index == _model_id) return;

   dsp::ReverbModel * incoming = model_for_id (fx_index);

   // switching again mid-fade strands the outgoing voice un-cleared. Retire it
   // here, unless it is the one coming back: then its tail is still live and
   // reusing it is what makes an A->B->A flick sound continuous.
   if (_prev_model && _prev_model != incoming) _prev_model->reset ();

   _prev_model = (_active_model == incoming) ? nullptr : _active_model;
   _model_id = fx_index;
   _model_xfade = 0.f;

   // no reset here: voices are cleared on retirement, so the incoming one is
   // already clear and re-zeroing it would only blow the block deadline
   _active_model = incoming;
   _active_model->set_decay (_decay);
   _active_model->set_macro (_fx_param);
}

dsp::TailFx *   GlacialReverbDsp::tail_for_id (int id)
{
   switch (id)
   {
   case 1:  return &_tail_reso;
   case 2:  return &_tail_pitch;
   case 3:  return &_tail_chorus;
   case 4:  return &_tail_granular;
   case 5:  return &_tail_droplet;
   case 6:  return &_tail_sub;
   case 7:  return &_tail_reverse;
   case 8:  return &_tail_chord;
   case 9:  return &_tail_formant;
   default: return &_tail_none;
   }
}

void  GlacialReverbDsp::set_tail_fx (int tail_index)
{
   if (tail_index < 0) tail_index = 0;
   if (tail_index >= nbr_tail_fx) tail_index = nbr_tail_fx - 1;
   if (tail_index == _tail_id) return;

   dsp::TailFx * incoming = tail_for_id (tail_index);

   if (_prev_tail && _prev_tail != incoming) _prev_tail->reset ();

   _prev_tail = (_active_tail == incoming) ? nullptr : _active_tail;
   _tail_id = tail_index;
   _tail_xfade = 0.f;

   _active_tail = incoming;
   _active_tail->set_amount (_tail_amount);
}

void  GlacialReverbDsp::set_tail_amount (float amount)
{
   if (amount < 0.f) amount = 0.f;
   if (amount > 1.f) amount = 1.f;
   _tail_amount = amount;
   if (_active_tail) _active_tail->set_amount (amount);
}

void  GlacialReverbDsp::set_rate (float rate)  { (void) rate; }
void  GlacialReverbDsp::set_depth (float depth) { (void) depth; }

void  GlacialReverbDsp::set_duck (float duck)
{
   if (duck < 0.f) duck = 0.f;
   if (duck > 1.f) duck = 1.f;
   _duck = duck;
}

void  GlacialReverbDsp::process (float * const out [], const float * const in [], std::size_t size)
{
   auto in_left  = in [0];
   auto in_right = in [1];
   auto out_left  = out [0];
   auto out_right = out [1];

   float feedback_frozen;
   const bool freeze_off_release = (! _freeze && _frozen_out_cur > 0.001f);

   if (_freeze)
   {
      feedback_frozen = 1.0f;
      _frozen_input_gain.target = _frozen_mix;
      _frozen_out_target = _frozen_level;
   }
   else
   {
      feedback_frozen = 0.3f + _decay * 0.65f;
      if (feedback_frozen > 0.99f) feedback_frozen = 0.99f;
      _frozen_input_gain.target = freeze_off_release ? 0.0f : 1.0f;
      _frozen_out_target = 0.0f;
   }

   _wet.target = _mix;
   _tank_inject_gain.target = (_freeze && ! _tank_enabled) ? 0.f : 1.f;

   if (_freeze) _freeze_hold_s += float (size) / _sample_freq;
   else         _freeze_hold_s = 0.f;

   float freeze_t = _freeze_hold_s / 8.f;
   if (freeze_t < 0.f) freeze_t = 0.f;
   if (freeze_t > 1.f) freeze_t = 1.f;
   float frozen_lp = 18000.f - 12000.f * freeze_t;
   if (frozen_lp < 3000.f) frozen_lp = 3000.f;
   _reverb_frozen.set_low_pass_freq (frozen_lp);
   _reverb_frozen.set_feedback (feedback_frozen);

   _active_model->set_decay (_decay);
   _active_model->set_macro (_fx_param);

   const float frozen_attack_coeff
      = 1.f - std::exp (-1.f / (_sample_freq * 0.010f));
   const float frozen_release_time = 0.5f + _decay * 4.0f;
   const float frozen_release_coeff
      = 1.f - std::exp (-1.f / (_sample_freq * frozen_release_time));

   for (std::size_t i = 0 ; i < size ; ++i)
   {
      _wet.process ();
      _frozen_input_gain.process ();
      _tank_inject_gain.process ();

      {
         float fo_coeff = (_frozen_out_target > _frozen_out_cur)
            ? frozen_attack_coeff
            : frozen_release_coeff;
         _frozen_out_cur += fo_coeff * (_frozen_out_target - _frozen_out_cur);
      }

      _model_xfade += _xfade_inc;
      if (_model_xfade > 1.f) _model_xfade = 1.f;
      _tail_xfade += _xfade_inc;
      if (_tail_xfade > 1.f) _tail_xfade = 1.f;

      float dry_l = in_left  [i];
      float dry_r = in_right [i];

      float pre_delay_target_spl = 1.f + (_pre_delay * _pre_delay * _pre_delay_max_spl);
      _pre_delay_spl += 0.0025f * (pre_delay_target_spl - _pre_delay_spl);

      int pd_wp = _pre_delay_write_pos;
      _pre_delay_buf_l [pd_wp] = dry_l;
      _pre_delay_buf_r [pd_wp] = dry_r;

      auto read_pre_delay = [&] (const float * buf, float delay) {
         if (delay < 1.f) delay = 1.f;
         float max_delay = float (_pre_delay_buf_size - 2);
         if (delay > max_delay) delay = max_delay;

         float read_pos = float (pd_wp) - delay;
         while (read_pos < 0.f) read_pos += float (_pre_delay_buf_size);
         while (read_pos >= float (_pre_delay_buf_size)) read_pos -= float (_pre_delay_buf_size);

         int idx0 = int (read_pos);
         int idx1 = idx0 + 1;
         if (idx1 >= _pre_delay_buf_size) idx1 = 0;
         float frac = read_pos - float (idx0);
         float x0 = buf [idx0];
         float x1 = buf [idx1];
         return x0 + (x1 - x0) * frac;
      };

      float pd_l = read_pre_delay (_pre_delay_buf_l, _pre_delay_spl);
      float pd_r = read_pre_delay (_pre_delay_buf_r, _pre_delay_spl);

      // Eurorack inputs are DC-coupled and every model's damper has unity DC
      // gain, so any input offset is integrated by the tank until the wet path
      // rails. Block it on the wet feed only; the dry stays a passthrough.
      float dc_l = pd_l - _dc_x_l + 0.9985f * _dc_y_l; _dc_x_l = pd_l; _dc_y_l = dc_l;
      float dc_r = pd_r - _dc_x_r + 0.9985f * _dc_y_r; _dc_x_r = pd_r; _dc_y_r = dc_r;
      pd_l = dc_l;
      pd_r = dc_r;

      ++_pre_delay_write_pos;
      if (_pre_delay_write_pos >= _pre_delay_buf_size) _pre_delay_write_pos = 0;

      float live_in_gain = static_cast <float> (_tank_inject_gain);
      dsp::StereoFrame mdl_in { pd_l * live_in_gain, pd_r * live_in_gain };
      dsp::StereoFrame live = _active_model->process (mdl_in);

      // keep rendering the outgoing model across the ramp: fading in the new one
      // alone would delete the tail, since a just-retired model is silent
      if (_prev_model)
      {
         dsp::StereoFrame old = _prev_model->process (mdl_in);
         live.left  = old.left  + _model_xfade * (live.left  - old.left);
         live.right = old.right + _model_xfade * (live.right - old.right);
         if (_model_xfade >= 1.f) { _prev_model->reset (); _prev_model = nullptr; }
      }

      dsp::StereoFrame tail = _active_tail->process (live);   // post-model tail effect
      if (_prev_tail)
      {
         dsp::StereoFrame old = _prev_tail->process (live);
         tail.left  = old.left  + _tail_xfade * (tail.left  - old.left);
         tail.right = old.right + _tail_xfade * (tail.right - old.right);
         if (_tail_xfade >= 1.f) { _prev_tail->reset (); _prev_tail = nullptr; }
      }
      live = tail;

      // key off the dry input only: including the tail makes the reverb duck
      // itself and hold down for the whole decay
      float abs_l  = std::fabs (dry_l);
      float abs_r  = std::fabs (dry_r);
      float level  = (abs_l > abs_r) ? abs_l : abs_r;
      if (level > _duck_env) _duck_env += _duck_atk * (level - _duck_env);
      else                   _duck_env += _duck_rel * (level - _duck_env);

      // DUCK is the depth; the envelope only gates it, so the pot stays usable
      // over its whole travel instead of saturating
      float duck_drive = _duck_env * 3.f;
      if (duck_drive > 1.f) duck_drive = 1.f;
      const float duck_reduction = _duck * 0.95f * duck_drive;

      float frozen_in_gain = static_cast <float> (_frozen_input_gain);
      auto rv_frozen = _reverb_frozen.process ({ pd_l * frozen_in_gain, pd_r * frozen_in_gain });

      float frozen_out_gain = _frozen_out_cur;
      float frozen_duck_gain = (_freeze && _duck > 0.f) ? 1.f - duck_reduction : 1.f;
      rv_frozen.left  *= frozen_out_gain * frozen_duck_gain;
      rv_frozen.right *= frozen_out_gain * frozen_duck_gain;

      // bound the whole wet path (model + tail + frozen pad) in one place, so no
      // combination of live tail and frozen pad can ever clip the output
      float wet_l = soft_wet (live.left  + rv_frozen.left);
      float wet_r = soft_wet (live.right + rv_frozen.right);

      float wet_gain = static_cast <float> (_wet);
      float dry_gain = 1.f;

      if (_duck > 0.f) wet_gain *= 1.f - duck_reduction;

      out_left  [i] = dry_gain * dry_l + wet_gain * wet_l;
      out_right [i] = dry_gain * dry_r + wet_gain * wet_r;
   }
}
