#pragma once

#include "index_of.h"
#include "max.h"

#include <limits>
#include <type_traits>
#include <cassert>
#include <utility>

namespace meta {
  namespace details {

    template <typename...>
    struct invoke_t;

    template <typename type, typename... rest_t>
    struct invoke_t<type, rest_t...>
    {
      static void destruct(size_t index, void* data) {
        if (0 == index)
          reinterpret_cast<type*>(data)->~type();
        else
          invoke_t<rest_t...>::destruct(index - 1, data);
      }

      static void copy_to(size_t index, const void* from, void* to) {
        if (0 == index)
          new (to) type(*reinterpret_cast<const type*>(from));
        else
          invoke_t<rest_t...>::copy_to(index - 1, from, to);
      }

      static void move_construct(size_t index, void* from, void* to) {
        if (0 == index)
          new (to) type(std::move(*reinterpret_cast<type*>(from)));
        else
          invoke_t<rest_t...>::move_construct(index - 1, from, to);
      }

      static void move_to(size_t index, void* from, void* to) {
        if (0 == index)
          *reinterpret_cast<type*>(to) = std::move(*reinterpret_cast<type*>(from));
        else
          invoke_t<rest_t...>::move_to(index - 1, from, to);
      }
    };

    template <>
    struct invoke_t<>
    {
      static void destruct(size_t, void*) { assert(false); }
      static void copy_to(size_t, const void*, void*) { assert(false); }
      static void move_construct(size_t, void*, void*) { assert(false); }
      static void move_to(size_t, void*, void*) { assert(false); }
    };

  } // namespace details

  template<typename... option_t>
  struct variant_t
  {
  private:
    static constexpr size_t max_size = max<sizeof(option_t)...>::value;
    static constexpr size_t no_type = sizeof...(option_t);
    using data_t = typename std::aligned_union<max_size, option_t...>::type;
    using invoke_t = details::invoke_t<option_t...>;

  public:
    variant_t() = default;
    ~variant_t() { reset(); }

    variant_t(const variant_t& other) : type_m(other.type_m) {
      if (!empty())
        invoke_t::copy_to(type_m, &other.data_m, &data_m);
    }
    variant_t(variant_t&& other) : type_m(other.type_m)  {
      if (!empty())
        invoke_t::move_construct(type_m, &other.data_m, &data_m);
    }
    template<typename value_t>
    variant_t(value_t&& value)
      : type_m(index_of_v<typename std::remove_reference<value_t>::type, option_t...>) {
      using bare_t = typename std::remove_reference<value_t>::type;
      assert(!empty());
      new ((void*)&data_m) bare_t(std::forward<value_t>(value));
    }

    variant_t& operator =(const variant_t& other) {
      reset();
      type_m = other.type_m;
      if (!empty())
        invoke_t::copy_to(type_m, &other.data_m, &data_m);
      return *this;
    }
    variant_t& operator =(variant_t&& other) {
      if (type_m != other.type_m) {
          reset();
          type_m = other.type_m;
          if (!empty())
            invoke_t::move_construct(type_m, &other.data_m, &data_m);
        }
      else {
          if (!empty())
            invoke_t::move_to(type_m, &other.data_m, &data_m);
        }
      return *this;
    }


    bool empty() const
    {
      return type_m == no_type;
    }

    template<typename value_t>
    bool is() const
    {
      static_assert(-1 != index_of_v<value_t, option_t...>, "invalid type");
      return (!empty() && type_m == index_of_v<value_t, option_t...>);
    }

    template<typename value_t>
    const value_t& get() const
    {
      static_assert(-1 != index_of_v<value_t, option_t...>, "invalid type");
      assert(is<value_t>());
      return *static_cast<value_t*>((void*)&data_m);
    }

    template<typename value_t>
    void set(value_t&& value)
    {
      using bare_t = typename std::remove_reference<value_t>::type;
      reset();
      type_m = index_of_v<bare_t, option_t...>;
      assert(!empty());
      new ((void*)&data_m) bare_t(std::forward<value_t>(value));
    }

    template<typename value_t, typename... arg_t>
    void emplace(arg_t&&... args)
    {
      reset();
      type_m = index_of_v<value_t, option_t...>;
      assert(!empty());
      new (&data_m) value_t(std::forward<arg_t>(args)...);
    }

    void reset()
    {
      if (empty()) return;
      invoke_t::destruct(type_m, &data_m);
    }

  private:
    int type_m = no_type;
    data_t data_m;
  };

  template<typename type>
  using optional_t = variant_t<type>;

} // namespace meta
