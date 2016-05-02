#pragma once

#include <windows.h>
#undef max
#undef min

#include <utility>

namespace windows {

struct unique_handle_t;
struct shared_handle_t;

struct handle_wrapper_t {
  handle_wrapper_t() = default;
  ~handle_wrapper_t() = default;

  handle_wrapper_t(const handle_wrapper_t&) = default;
  handle_wrapper_t& operator= (const handle_wrapper_t&) = default;

  handle_wrapper_t(handle_wrapper_t&& other) = default;
  handle_wrapper_t& operator= (handle_wrapper_t&& other) = default;

  bool valid() const { return handle_m != INVALID_HANDLE_VALUE; }

protected:
  HANDLE handle_m = INVALID_HANDLE_VALUE;
};

struct unique_handle_t : handle_wrapper_t {
  unique_handle_t() = default;
  ~unique_handle_t() {
    if (valid()) {
        ::CloseHandle(handle_m);
      }
  }

  unique_handle_t(const unique_handle_t&) = delete;
  unique_handle_t& operator= (const unique_handle_t&) = delete;

  unique_handle_t(unique_handle_t&& other) {
    handle_m = other.handle_m;
    other.handle_m = INVALID_HANDLE_VALUE;
  }
  unique_handle_t& operator= (unique_handle_t&& other) {
    using namespace std;
    swap(handle_m, other.handle_m);
    return *this;
  }

  void share(const unique_handle_t &); // overload not implemeted

  friend struct shared_handle_t;
};

struct shared_handle_t : handle_wrapper_t {
  void share(const unique_handle_t& source) {
    handle_m = source.handle_m;
  }
};

} // namespace windows
