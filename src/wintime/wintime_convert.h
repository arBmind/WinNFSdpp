#pragma once

#include <Windows.h>
#include <time.h>
#include <utility>
#include <cstdint>

namespace wintime {

  struct unix_time_t {
    uint64_t seconds;
    uint32_t nanoseconds;

    bool operator == (const unix_time_t& other) const {
      return seconds == other.seconds
          && nanoseconds == other.nanoseconds;
    }
    bool operator != (const unix_time_t& other) const {
      return !((*this) == other);
    }
  };

  const uint64_t filetime_Jan1970 = 116444736000000000u; // '100 nanosecons since 1601
  const uint64_t filetime_Second  =           10000000u;
  const uint64_t nanosecond_Filetime = 100u; // filetime stores 100 nanoseconds

  inline unix_time_t convert_LARGE_INTEGER_to_unix_time(const LARGE_INTEGER& filetime) {
    unix_time_t result;
    auto combined = filetime.QuadPart;
    result.seconds = (combined - filetime_Jan1970) / filetime_Second;
    result.nanoseconds = (combined % filetime_Second) * nanosecond_Filetime;
    return result;
  }

  inline unix_time_t convert_FILETIME_to_unix_time(const FILETIME& filetime) {
    LARGE_INTEGER large_integer;
    large_integer.LowPart = filetime.dwLowDateTime;
    large_integer.HighPart = filetime.dwHighDateTime;
    return convert_LARGE_INTEGER_to_unix_time(large_integer);
  }

  inline LARGE_INTEGER convert_unix_time_to_LARGE_INTEGER(const unix_time_t& unixtime) {
    LARGE_INTEGER result;
    result.QuadPart  = (unixtime.nanoseconds / nanosecond_Filetime) % filetime_Second;
    result.QuadPart += (unixtime.seconds * filetime_Second) + filetime_Jan1970;
    return result;
  }

  inline FILETIME convert_unix_time_to_FILETIME(const unix_time_t& unixtime) {
    FILETIME result;
    auto large_integer = convert_unix_time_to_LARGE_INTEGER(unixtime);
    result.dwLowDateTime = large_integer.LowPart;
    result.dwHighDateTime = large_integer.HighPart;
    return result;
  }

} // namespace wintime
