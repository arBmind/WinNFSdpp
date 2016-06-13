#pragma once

#include "binary.h"

#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <cassert>
#include <iomanip>
#include <sstream>

struct binary_reader_t
{
  using it = const uint8_t*;

  binary_reader_t() = default;
  binary_reader_t(it begin, it end)
    : begin_m(begin), end_m(end)
  {}

  static binary_reader_t binary(const binary_t& binary) {
    if (binary.empty())
        return binary_reader_t();
    auto begin = &binary[0];
    auto end = begin + binary.size();
    return binary_reader_t(begin, end);
  }

  size_t size() const { return end_m - begin_m; }
  bool empty() const { return end_m == begin_m; }

  bool valid() const { return begin_m < end_m; }
  bool has_size(size_t needed) const { return size() >= needed; }

  uint8_t get8(size_t offset) const {
    assert(has_size(offset));
    return begin_m[offset];
  }

  uint32_t get32(size_t offset) const;
  uint64_t get64(size_t offset) const;

  template<typename value_t>
  value_t get32(size_t offset) const { return static_cast<value_t>(get32(offset)); }

  binary_reader_t get_reader(size_t offset) const {
    return { begin_m + offset, end_m };
  }

  binary_reader_t get_reader(size_t offset, size_t size) const {
    return { begin_m + offset, begin_m + offset + size };
  }

  void get_binary(size_t offset, uint8_t* data, size_t size) const;

  template<size_t size>
  void get_binary(size_t offset, const uint8_t (&data)[size]) const {
    get_binary(offset, &data[0], size);
  }

  template<size_t size>
  void get_binary(size_t offset, std::array<uint8_t, size>& data) const {
    get_binary(offset, &data[0], size);
  }

  binary_t get_binary(size_t offset, size_t size) const {
    binary_t result;
    result.resize(size);
    get_binary(offset, &result[0], size);
    return result;
  }

  std::string get_string(size_t offset, size_t size) const {
    std::string result;
    result.resize(size);
    get_binary(offset, (uint8_t*)&result[0], size);
    return result;
  }

  std::wstring get_wstring(size_t offset, size_t size) const {
    std::wstring result;
    result.resize(size);
    get_binary(offset, (uint8_t*)&result[0], sizeof(wchar_t) * size);
    return result;
  }

  std::string get_hex_string(size_t offset, size_t size) const{
      std::stringstream s; s << "0x";
      for (auto i = begin_m; i != end_m; ++i){
          s << std::setfill ('0') << std::setw(sizeof(*i)*2) << std::hex << +*i;
      }
      return s.str();
  }

private:
  it begin_m = nullptr;
  it end_m = nullptr;
};
