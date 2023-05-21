// Scoped simpled profiler
#ifndef SCOPED_PROFILER_HPP
#define SCOPED_PROFILER_HPP

#include <cstdint>
#include "esp32_clock.hpp"

namespace gob
{

// Unit: CPU
class CpuCycle
{
  public:
    explicit CpuCycle(uint32_t& out) : _save(out) { __asm__ __volatile("rsr %0, ccount" : "=r"(_start_at) ); }
    ~CpuCycle()
    {
        __asm__ __volatile("rsr %0, ccount" : "=r"(_end_at) );
        _save = _end_at - _start_at;
    }
    
  private:
    uint32_t _start_at{}, _end_at{};
    uint32_t& _save;
};

// Unit: us
class EspCycle
{
  public:
    explicit EspCycle(uint32_t& out) : _save(out) { _start_at = ESP32Clock::now(); }
    ~EspCycle()
    {
        _end_at = ESP32Clock::now();
        auto d = _end_at - _start_at;
        _save = (uint32_t)std::chrono::duration_cast<ESP32Clock::duration>(d).count();
    }
    
  private:
    ESP32Clock::time_point _start_at{}, _end_at{};
    uint32_t& _save;
};

//using Cycle = CpuCycle; // Unit:CPU
using Cycle = EspCycle; // Unit:us

//
}

#define GOB_CONCAT_AGAIN(a,b) a ## b
#define GOB_CONCAT(a,b) GOB_CONCAT_AGAIN(a,b)

#ifdef ENABLE_PROFILE
# define ScopedProfile(val) gob::Cycle GOB_CONCAT(pf_, __LINE__) ((val))
#else
# define ScopedProfile(val) /* nop */
#endif

#endif
