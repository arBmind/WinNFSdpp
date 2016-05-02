#pragma once

#include <stddef.h>

namespace meta {

  template <typename type, typename... types>
  struct index_of;

  template <typename type, typename... types>
  struct index_of<type, type, types...> {
    constexpr static size_t value = 0;
  };

  template <typename type, typename other, typename... types>
  struct index_of<type, other, types...> {
    constexpr static size_t value = index_of<type, types...>::value + 1;
  };

  template <typename type, typename... types>
  constexpr size_t index_of_v = index_of<type, types...>::value;

} // namespace meta
