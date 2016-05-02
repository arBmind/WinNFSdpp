#include "binary_reader.h"



uint32_t binary_reader_t::get32(size_t offset) const {
  uint32_t result = 0;
  for (auto i = 0u; i < sizeof(result); ++i) {
      result = (result << 8) + get8(offset + i);
    }
  return result;
}

uint64_t binary_reader_t::get64(size_t offset) const {
  uint64_t result = 0;
  for (auto i = 0u; i < sizeof(result); ++i) {
      result = (result << 8) + get8(offset + i);
    }
  return result;
}

void binary_reader_t::get_binary(size_t offset, uint8_t* data, size_t size) const {
  for (auto i = 0u; i < size; ++i) {
      data[i] = get8(offset + i);
    }
}
