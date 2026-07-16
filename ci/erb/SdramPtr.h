/*****************************************************************************

     ci/erb/SdramPtr.h
     Heap-backed stand-in for erb::SdramPtr, so the DSP compiles and runs off
     the target for CI. Same ownership + value-init semantics as the real one.

*Tab=3***********************************************************************/

#pragma once

#include <cstddef>
#include <new>
#include <utility>

namespace erb
{

template <typename T>
class SdramPtr
{
public:
   using element_type = T;
   using pointer      = T *;
   using reference    = T &;

                  SdramPtr () = default;
   explicit       SdramPtr (pointer p) : _ptr (p) {}
                  SdramPtr (SdramPtr && r) : _ptr (r._ptr) { r._ptr = nullptr; }
                  ~SdramPtr () { release (); }

   SdramPtr &     operator = (SdramPtr && r)
   {
      if (this != &r) { release (); _ptr = r._ptr; r._ptr = nullptr; }
      return *this;
   }

   pointer        get () const { return _ptr; }
   explicit       operator bool () const { return _ptr != nullptr; }
   reference      operator * () const { return *_ptr; }
   pointer        operator -> () const { return _ptr; }

private:
   static constexpr std::size_t alignment ()
   {
      return alignof (T) < 16 ? 16 : alignof (T);
   }
   void           release ()
   {
      if (! _ptr) return;
      _ptr->~T ();
      ::operator delete (_ptr, std::align_val_t (alignment ()));
      _ptr = nullptr;
   }
   pointer        _ptr = nullptr;

                  SdramPtr (const SdramPtr &) = delete;
   SdramPtr &     operator = (const SdramPtr &) = delete;

   template <typename U, class... Args>
   friend SdramPtr <U> make_sdram (Args &&...);
};

// value-initialise, exactly as the framework does: the Buffers structs rely on
// their {} member initialisers to zero the delay lines
template <typename T, class... Args>
SdramPtr <T>   make_sdram (Args &&... args)
{
   constexpr std::size_t a = alignof (T) < 16 ? 16 : alignof (T);
   void * raw = ::operator new (sizeof (T), std::align_val_t (a));
   return SdramPtr <T> (new (raw) T (std::forward <Args> (args)...));
}

}  // namespace erb
