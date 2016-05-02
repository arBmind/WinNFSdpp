#include "binary_builder.h"

void binary_builder_t::clear()
{
  binary_m.clear();
  offset_m = 0;
}

void binary_builder_t::seek_end()
{
  offset_m = binary_m.size();
}

void binary_builder_t::seek_to(size_t offset)
{
  offset_m = offset;
}

void binary_builder_t::append8(uint8_t value)
{
  if (offset_m >= size()) {
      binary_m.push_back(value);
    }
  else {
      binary_m[offset_m] = value;
    }
  ++offset_m;
}

void binary_builder_t::append32(uint32_t value)
{
  for (auto n = sizeof(value); n--;) {
      append8((value >> (n*8)) & 255);
    }
}

void binary_builder_t::append64(uint64_t value)
{
  for (auto n = sizeof(value); n--;) {
      append8((value >> (n*8)) & 255);
    }
}

void binary_builder_t::append_binary(const uint8_t* binary, size_t size)
{
  auto binary_size = binary_m.size();
  while (offset_m < binary_size && size > 0) {
      binary_m[offset_m] = *binary;
      ++offset_m;
      ++binary;
      --size;
    }
  if (size > 0) {
      binary_m.insert(binary_m.end(), binary, binary + size);
      offset_m += size;
    }
}
