// ESP32 clock compatible with std::chrono::any_clock
#ifndef ESP32_CLOCK_HPP
#define ESP32_CLOCK_HPP

#include <chrono>
#include <esp_system.h>

// ESP32 clock compatible with std::chrono::any_clock
struct ESP32Clock
{
    using duration = std::chrono::microseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<ESP32Clock, duration>;
    static const bool is_steady = true;
    static time_point now() noexcept { return time_point( duration(static_cast<rep>(esp_timer_get_time())) ); }
    static int64_t raw() noexcept { return esp_timer_get_time(); }
};
#endif

