#pragma once

#include <utility>

struct wsa_session_t {
  explicit wsa_session_t(int major, int minor);
  ~wsa_session_t();

  wsa_session_t() = default;
  wsa_session_t(const wsa_session_t&) = delete;
  wsa_session_t& operator= (const wsa_session_t&) = delete;

  wsa_session_t(wsa_session_t&& other) {
    activated_m = other.activated_m;
    other.activated_m = false;
  }
  wsa_session_t& operator= (wsa_session_t&& other) {
    using namespace std;
    swap(activated_m, other.activated_m);
    return *this;
  }

private:
  bool activated_m = false;
};
