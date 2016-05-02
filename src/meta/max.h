#pragma once

#include <stddef.h>

namespace meta {

  template <size_t... sizes>
  struct max;

  template <size_t size, size_t... sizes>
  struct max<size, sizes...> {
  private:
    static constexpr size_t temp = max<sizes...>::value;
  public:
    static constexpr size_t value = temp > size ? temp : size;
  };

  template <>
  struct max<> {
    static constexpr size_t value = 0;
  };

} // namespace meta
