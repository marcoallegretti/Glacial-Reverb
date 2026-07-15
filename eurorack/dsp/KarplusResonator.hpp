#pragma once

namespace dsp
{

inline void  KarplusResonator::process (float in_l, float in_r, float & out_l, float & out_r)
{
   if (_mix <= 0.f || _buf_size <= 2 || _delay_samples <= 1.f)
   {
      out_l = in_l;
      out_r = in_r;
      return;
   }

   int write_pos = _write_pos;

   float read_pos = float (write_pos) - _delay_samples;
   float size_f = float (_buf_size);
   while (read_pos < 0.f)      read_pos += size_f;
   while (read_pos >= size_f)  read_pos -= size_f;

   int idx0 = int (read_pos);
   int idx1 = idx0 + 1;
   if (idx1 >= _buf_size) idx1 = 0;
   float frac = read_pos - float (idx0);

   float dl0 = _buf_l [idx0];
   float dl1 = _buf_l [idx1];
   float dr0 = _buf_r [idx0];
   float dr1 = _buf_r [idx1];

   float res_l = dl0 + (dl1 - dl0) * frac;
   float res_r = dr0 + (dr1 - dr0) * frac;

   _low_l += _damp * (res_l - _low_l);
   _low_r += _damp * (res_r - _low_r);
   float fb_l = _low_l;
   float fb_r = _low_r;

   _buf_l [write_pos] = in_l + fb_l * _feedback;
   _buf_r [write_pos] = in_r + fb_r * _feedback;

   ++_write_pos;
   if (_write_pos >= _buf_size) _write_pos = 0;

   float mix = _mix;
   out_l = in_l * (1.f - mix) + res_l * mix;
   out_r = in_r * (1.f - mix) + res_r * mix;
}

} // namespace dsp
