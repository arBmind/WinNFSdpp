#pragma once

#include <string>
#include <cstring>
#include <wchar.h>

struct convert {
  struct state_t {
    state_t() {
      ::memset(&state_m, 0, sizeof state_m);
    }

    operator mbstate_t& () {
      return state_m;
    }

  private:
    mbstate_t state_m;
  };

  // ---- to_wstring ----
  static inline std::wstring to_wstring(const std::string& src) {
    return to_wstring_with_offset(src, 0);
  }

  static inline std::wstring to_wstring_with_offset(const std::string& src, size_t offset) {
    state_t state;
    return to_wstring_with_offset(src, offset, state);
  }

  static inline std::wstring to_wstring_with_offset(const std::string& src, size_t offset, state_t &state) {
    auto wstring_length = to_wstring_length_with_offset(src, offset, state);
    return to_wstring_with_offset_and_length(src, offset, wstring_length, state);
  }

  static inline size_t to_wstring_length_with_offset(const std::string& src, size_t offset, mbstate_t &state) {
    size_t result;
    auto cstr = src.c_str() + offset;
    auto clen = src.length() - offset;
    mbsrtowcs_s(&result,
                nullptr, 0,
                &cstr, clen, &state);;
    return result;
  }

  static inline std::wstring to_wstring_with_offset_and_length(const std::string& src, size_t offset, size_t wstring_length, mbstate_t &state) {
    std::wstring result;
    auto cstr = src.c_str() + offset;
    auto clen = src.length() - offset;
    result.reserve(wstring_length);
    result.resize(wstring_length - 1);
    size_t converted;
    mbsrtowcs_s(&converted,
                &result[0], wstring_length,
                &cstr, clen, &state);
    return result;
  }

  // ---- to_string ----
  static inline std::string to_string(const std::wstring& src) {
    return to_string_with_offset(src, 0);
  }

  static inline std::string to_string_with_offset(const std::wstring& src, size_t offset) {
    state_t state;
    return to_string_with_offset(src, offset, state);
  }

  static inline std::string to_string_with_offset(const std::wstring& src, size_t offset, state_t &state) {
    auto string_length = to_string_length_with_offset(src, offset, state);
    return to_string_with_offset_and_length(src, offset, string_length, state);
  }

  static inline size_t to_string_length(const std::wstring& src, state_t &state) {
    return to_string_length_with_offset(src, 0, state);
  }

  static inline size_t to_string_length_with_offset(const std::wstring& src, size_t offset, mbstate_t &state) {
    size_t result;
    auto cstr = src.c_str() + offset;
    auto clen = src.length() - offset;
    wcsrtombs_s(&result,
                nullptr, 0,
                &cstr, clen, &state);;
    return result;
  }

  static inline std::string to_string_with_length(const std::wstring& src, size_t string_length, state_t &state) {
    return to_string_with_offset_and_length(src, 0, string_length, state);
  }

  static inline std::string to_string_with_offset_and_length(const std::wstring& src, size_t offset, size_t string_length, mbstate_t &state) {
    std::string result;
    auto cstr = src.c_str() + offset;
    auto clen = src.length() - offset;
    result.reserve(string_length);
    result.resize(string_length - 1);
    size_t converted;
    wcsrtombs_s(&converted,
                &result[0], string_length,
                &cstr, clen, &state);
    return result;
  }


};
