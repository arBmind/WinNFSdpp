#pragma once

#include <atomic>
#include <shared_mutex>
#include <vector>
#include <algorithm>
#include <cassert>
#include <glog/logging.h>

struct mount_aliases_t {
  using source_t = uint32_t;
  using mount_id_t = uint32_t;
  using windows_path_t = std::wstring;
  using alias_path_t = std::string;
  using windows_alias_path_pair_t = std::pair<windows_path_t, alias_path_t>;
  using alias_vector_t = std::vector<windows_alias_path_pair_t>;

public:
  windows_path_t resolve(const alias_path_t& alias_path) const {
    std::shared_lock<std::shared_timed_mutex> lock(store_mutex_m);
    return by_alias_path_safe(alias_path);
  }

  static bool check_alias_path(const alias_path_t& alias_path) {
    if (alias_path.empty()) return false; // empty is not valid
    if (alias_path[0] != '/') return false; // has to start with slash
    if (std::any_of(alias_path.begin(), alias_path.end(), [](char chr) {
      return chr < 32;
    })) return false; // invalid character
    return true;
  }

  static windows_path_t alias_subpath_to_windows(const alias_path_t&);
  static alias_path_t windows_to_alias_path(const windows_path_t&);

public:
  source_t create_source() {
    return next_source_m++;
  }

  bool add(source_t source, const windows_path_t& windows_path, const alias_path_t& alias_path = {}) {
    CHECK(alias_path.empty() || check_alias_path(alias_path));
    std::unique_lock<std::shared_timed_mutex> lock(store_mutex_m);
    return add_safe(source, windows_path, alias_path);
  }

  void set(source_t source, const alias_vector_t& alias_vector) {
    CHECK(std::all_of(alias_vector.begin(), alias_vector.end(), [] (const windows_alias_path_pair_t& pair) {
        return pair.second.empty() || check_alias_path(pair.second);
      }));
    std::unique_lock<std::shared_timed_mutex> lock(store_mutex_m);
    set_aliases_safe(source, alias_vector);
  }

  void clear(source_t source) {
    set(source, {});
  }

private:
  windows_path_t by_alias_path_safe(const alias_path_t&) const;

  bool add_safe(source_t, const windows_path_t&, const alias_path_t&);
  void set_aliases_safe(source_t, const alias_vector_t&);

private:
  struct entry_t {
    source_t source;
    alias_path_t alias_path; // path used by NFS to mount this
    windows_path_t windows_path; // base path for windows
  };
  using store_t = std::vector<entry_t>;

  std::atomic<source_t> next_source_m {1};

  mutable std::shared_timed_mutex store_mutex_m;
  store_t store_m;
};
