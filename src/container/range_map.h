#pragma once

#include <vector>
#include <cstdint>

template <typename T>
struct range_map_t {
  using key_t = uint32_t;
  using store_t = std::vector<T>;

  const T& operator[] (size_t index) const { return store_m[index - offset_m]; }
  T& operator[] (size_t index) { return store_m[index - offset_m]; }

  bool empty() const { return store_m.empty(); }
  bool contains(size_t index) const { return index >= range_start() && index < range_end(); }
  size_t size() const { return store_m.size(); }
  key_t range_start() const { return offset_m; }
  key_t range_end() const { return offset_m + size(); }

  void assign(key_t offset, const store_t& store) { offset_m = offset; store_m = store; }

  T& set(key_t index, const T& data) {
    if (empty()) offset_m = index;
    extend_start(index);
    extend_end(index);
    return (*this)[index] = data;
  }

  void extend_start(key_t index) {
    if (range_start() <= index) return;
    store_m.insert(store_m.begin(), range_start() - index, {});
  }

  void extend_end(key_t index) {
    if (range_end() > index) return;
    store_m.insert(store_m.end(), 1 + index - range_end(), {});
  }

private:
  key_t offset_m = 0;
  store_t store_m;
};
