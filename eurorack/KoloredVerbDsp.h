/*****************************************************************************

     KoloredVerbDsp.h
     Reverb wrapper: freeze pad, duck, mix and pre-delay around the active model.

*Tab=3***********************************************************************/

#pragma once

/*\\ INCLUDE FILES \\\\*/

#include "dsp/GainRamp.h"
#include "dsp/ReverbSc.h"
#include "dsp/ReverbModel.h"
#include "dsp/ReverbHall.h"
#include "dsp/ReverbPlate.h"
#include "dsp/ReverbShimmer.h"
#include "dsp/ReverbCloud.h"
#include "dsp/ReverbAmbient.h"
#include "dsp/ReverbSpring.h"
#include "dsp/ReverbAbyss.h"
#include "dsp/ReverbSpringTank.h"
#include "dsp/ReverbSpringDisp.h"
#include "dsp/ReverbPlateVintage.h"
#include "dsp/ReverbPlateDisp.h"
#include "dsp/ReverbChamber.h"

#include "dsp/TailFx.h"
#include "dsp/TailFxResonator.h"
#include "dsp/TailFxPitch.h"
#include "dsp/TailFxChorus.h"
#include "dsp/TailFxGranular.h"
#include "dsp/TailFxDroplet.h"
#include "dsp/TailFxSub.h"
#include "dsp/TailFxReverse.h"
#include "dsp/TailFxChord.h"
#include "dsp/TailFxFormant.h"

#include <cstddef>


class KoloredVerbDsp
{

/*\\ PUBLIC \\\\*/

public:
   KoloredVerbDsp (float sample_freq);
   ~KoloredVerbDsp ();

   void  set_mix (float mix);
   void  set_decay (float decay);
   void  set_pre_delay (float pre_delay);
   void  set_frozen_level (float level);
   void  set_frozen_mix (float mix);
   void  set_freeze (bool freeze);
   void  set_fx (float fx);
   void  set_rate (float rate);
   void  set_depth (float depth);
   void  set_duck (float duck);
   void  set_tank (bool enabled);
   void  set_fx_type (int fx_index);
   void  set_tail_fx (int tail_index);
   void  set_tail_amount (float amount);

   void  process (float * const out [], const float * const in [], std::size_t size);

   static constexpr int nbr_models  = 12;  // 6 families, 12 voices
   static constexpr int nbr_tail_fx = 10;  // +Reverse, Chord, Formant


/*\\ PRIVATE \\\\*/

private:
   float         _sample_freq;

   float         _mix          = 0.f;
   float         _decay        = 0.5f;
   float         _pre_delay    = 0.f;
   float         _frozen_level = 1.f;
   float         _frozen_mix   = 0.5f;
   bool          _freeze       = false;
   bool          _tank_enabled = true;
   float         _duck         = 0.f;
   float         _duck_env     = 0.f;
   float         _duck_atk     = 0.f;
   float         _duck_rel     = 0.f;
   float         _freeze_hold_s = 0.f;

   float         _fx_param     = 0.f;
   float         _tail_amount  = 0.f;
   float         _model_xfade  = 1.f;
   float         _tail_xfade   = 1.f;
   float         _xfade_inc    = 0.f;
   int           _model_id     = 0;
   int           _tail_id      = 0;

   dsp::GainRamp _wet;
   dsp::GainRamp _frozen_input_gain;
   dsp::GainRamp _tank_inject_gain;

   float         _frozen_out_cur    = 0.f;
   float         _frozen_out_target = 0.f;

   // Selectable live reverb models + the always-on freeze pad tank.
   dsp::ReverbHall    _hall;
   dsp::ReverbPlate   _plate;
   dsp::ReverbShimmer _shimmer;
   dsp::ReverbCloud   _cloud;
   dsp::ReverbAmbient _ambient;
   dsp::ReverbSpring  _spring;
   dsp::ReverbAbyss   _abyss;
   dsp::ReverbSpringTank  _spring_tank;
   dsp::ReverbSpringDisp  _spring_disp;
   dsp::ReverbPlateVintage _plate_vintage;
   dsp::ReverbPlateDisp   _plate_disp;
   dsp::ReverbChamber     _chamber;
   dsp::ReverbModel * _active_model = nullptr;
   dsp::ReverbModel * _prev_model   = nullptr;   // rendered until _model_xfade hits 1

   dsp::TailFxNone      _tail_none;
   dsp::TailFxResonator _tail_reso;
   dsp::TailFxPitch     _tail_pitch;
   dsp::TailFxChorus    _tail_chorus;
   dsp::TailFxGranular  _tail_granular;
   dsp::TailFxDroplet   _tail_droplet;
   dsp::TailFxSub       _tail_sub;
   dsp::TailFxReverse   _tail_reverse;
   dsp::TailFxChord     _tail_chord;
   dsp::TailFxFormant   _tail_formant;
   dsp::TailFx *        _active_tail = nullptr;
   dsp::TailFx *        _prev_tail   = nullptr;   // rendered until _tail_xfade hits 1

   dsp::ReverbSc _reverb_frozen;

   float *       _pre_delay_buf_l  = nullptr;
   float *       _pre_delay_buf_r  = nullptr;
   int           _pre_delay_buf_size = 0;
   int           _pre_delay_write_pos = 0;
   float         _pre_delay_max_spl = 0.f;
   float         _pre_delay_spl = 1.f;

   float         _dc_x_l = 0.f, _dc_x_r = 0.f, _dc_y_l = 0.f, _dc_y_r = 0.f;

   dsp::ReverbModel * model_for_id (int id);
   dsp::TailFx *      tail_for_id (int id);

               KoloredVerbDsp () = delete;
               KoloredVerbDsp (const KoloredVerbDsp & rhs) = delete;
               KoloredVerbDsp (KoloredVerbDsp && rhs) = delete;
   KoloredVerbDsp & operator = (const KoloredVerbDsp & rhs) = delete;
   KoloredVerbDsp & operator = (KoloredVerbDsp && rhs) = delete;
};
