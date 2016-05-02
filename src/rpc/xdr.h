#pragma once

#include "binary/binary_reader.h"
#include "binary/binary_builder.h"

#include <limits>
#include <cassert>
#include <string>

namespace xdr {
  using string_t = std::string;

  struct opaque_reader_t;
  template <size_t max_size = std::numeric_limits<size_t>::max()>
  opaque_reader_t opaque_reader(const binary_reader_t& reader, size_t offset);

  struct opaque_reader_t : binary_reader_t {
    opaque_reader_t() = default;
    bool valid() const {
      return binary_reader_t::valid() && valid_size_m;
    }

    size_t read_size() const {
      auto screw = size() & 3;
      return 4 + size() + (0 == screw ? 0 : 4 - screw);
    }

    std::string to_string() const {
      assert(valid());
      return get_string(0, size());
    }

    binary_t to_binary() const {
      assert(valid());
      return get_binary(0, size());
    }

  private:
    template <size_t max_size>
    friend opaque_reader_t opaque_reader(const binary_reader_t& reader, size_t offset);

    opaque_reader_t(const binary_reader_t& reader, bool valid_size)
      : binary_reader_t(reader)
      , valid_size_m(valid_size)
    {}

  private:
    bool valid_size_m = false;
  };

  template <size_t max_size>
  opaque_reader_t opaque_reader(const binary_reader_t& reader, size_t offset) {
    auto size = reader.has_size(offset + 4) ? reader.get32(offset) : 0;
    return { reader.get_reader(offset + 4, size), size <= max_size };
  }

  inline void write_opaque(binary_builder_t& builder, const uint8_t* data, size_t size) {
    builder.append32(size);
    builder.append_binary(data, size);
    for (; (size & 3) != 0; ++size) {
        builder.append8(0); // align to 4 bytes
      }
  }

  template<size_t max_size = std::numeric_limits<size_t>::max()>
  bool write_opaque_binary(binary_builder_t& builder, const binary_t& binary) {
    auto size = std::min(max_size, binary.size());
    write_opaque(builder, binary.empty() ? nullptr : &binary[0], size);
    return (size <= max_size);
  }

  template<size_t max_size = std::numeric_limits<size_t>::max()>
  bool write_opaque_string(binary_builder_t& builder, const std::string& str) {
    auto size = std::min(max_size, str.size());
    write_opaque(builder, str.empty() ? nullptr : (const uint8_t*)&str[0], size);
    return (str.size() <= max_size);
  }

  template<typename type, typename callback_t>
  void write_list(binary_builder_t& builder, const type& list, const callback_t& callback) {
    auto it = begin(list);
    auto end_it = end(list);
    for (; it != end_it; ++it) {
        builder.append32(true);
        callback(builder, *it);
      }
    builder.append32(false);
  }

} // namespace xdr
