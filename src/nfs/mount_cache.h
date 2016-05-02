#pragma once

#include "winfs/winfs_object.h"

#include "binary/binary.h"

#include <shared_mutex>
#include <map>
#include <unordered_map>
#include <set>
#include <vector>
#include <string>

#include "GSL/string_span.h"

// gsl has a bug - std::less cannot find operators
struct mount_less {
  template<typename T>
  bool operator() (const T& a, const T& b) const {
    return a < b;
  }
};

struct assoc_iterator_less {
  template<typename T>
  bool operator() (const T& a, const T& b) const {
    return a->first < b->first;
  }
};

struct mount_filehandle_t {
  uint64_t mount_id;
  winfs::volume_file_id_t volume_file_id;

public:
  static bool is_valid(const binary_t& data) {
    return data.size() == sizeof(mount_filehandle_t);
  }
  static const mount_filehandle_t& view_binary(const binary_t& data) {
    return *reinterpret_cast<const mount_filehandle_t*>(&data[0]);
  }
  static mount_filehandle_t& create_in_binary(binary_t& data) {
    data.resize(sizeof(mount_filehandle_t));
    return *reinterpret_cast<mount_filehandle_t*>(&data[0]);
  }
};

struct mount_cache_t {
  using mount_id_t = uint64_t;
  using client_t = std::string;
  using client_view_t = gsl::cstring_span<>;
  using client_view_set_t = std::set<client_view_t, mount_less>;
  using query_path_t = std::string;
  using windows_path_t = std::wstring;
  using windows_path_view_t = gsl::cwstring_span<>;
  struct entry_t {
    winfs::unique_object_t directory;
    binary_t filehandle;
    windows_path_t windows_path;
    client_view_set_t clients;
  };
  using mount_map_t = std::unordered_map<mount_id_t, entry_t>;
  using mount_map_it = mount_map_t::iterator;
  using mount_map_cit = mount_map_t::const_iterator;
  using mounts_set_t = std::set<mount_map_it, assoc_iterator_less>;

  using windows_map_t = std::map<windows_path_view_t, mount_map_it, mount_less>;
  using query_map_t = std::map<query_path_t, mount_map_it>;

  using client_mounts_t = std::map<client_t, mounts_set_t>;

  // we need a longer lock session to decide what the mount changes
  struct mount_session_t {
    mount_session_t(mount_cache_t* self) : p(self) {}

    mount_map_cit end() const {
      return p->mount_map_m.end();
    }

    mount_map_cit find_query(const query_path_t& query_path) const {
      auto it = p->query_map_m.find(query_path);
      if (it != p->query_map_m.end()) return it->second;
      return p->mount_map_m.end();
    }

    mount_map_cit find_windows(const windows_path_t& windows_path) const {
      auto it = p->windows_map_m.find(windows_path);
      if (it != p->windows_map_m.end()) return it->second;
      return p->mount_map_m.end();
    }

    void mount_sender(const client_t& client, mount_map_cit mount_cit) const {
      mount_map_it mount_it = *reinterpret_cast<mount_map_it*>(&mount_cit);
      p->safe_mount_sender(client, mount_it);
    }

    void mount_sender_query(const client_t& client, mount_map_cit mount_cit, const query_path_t& query_path) const {
      mount_map_it mount_it = *reinterpret_cast<mount_map_it*>(&mount_cit);
      p->safe_mount_sender_query(client, mount_it, query_path);
    }

    mount_map_cit mount(const client_t& client, const query_path_t& query_path, const windows_path_t& windows_path) const {
      return p->safe_mount(client, query_path, windows_path);
    }

  private:
    mount_cache_t* p;
  };

public:
  std::pair<winfs::shared_object_t, binary_t> get(const mount_id_t& mount_id) const {
    std::pair<winfs::shared_object_t, binary_t> result;
    std::shared_lock<std::shared_timed_mutex> lock(mutex_m);
    auto it = mount_map_m.find(mount_id);
    if (it != mount_map_m.end()) {
        const entry_t& entry = it->second;
        result.first.share(entry.directory);
        result.second = entry.filehandle;
      }
    return result;
  }

  binary_t save() const {
    std::shared_lock<std::shared_timed_mutex> lock(mutex_m);
    return safe_save();
  }
  void restore(const binary_t& binary) {
    std::unique_lock<std::shared_timed_mutex> lock(mutex_m);
    safe_restore(binary);
  }

  template<typename callback_t>
  void mount_session(const callback_t& callback) {
    std::unique_lock<std::shared_timed_mutex> lock(mutex_m);
    callback(mount_session_t(this));
  }

  void unmount(const client_t& client, const query_path_t& query_path) {
    std::unique_lock<std::shared_timed_mutex> lock(mutex_m);
    safe_unmount(client, query_path);
  }

  void unmount_client(const client_t& client) {
    std::unique_lock<std::shared_timed_mutex> lock(mutex_m);
    safe_unmount_client(client);
  }

private:
  binary_t safe_save() const;
  void safe_restore(const binary_t&);

  void safe_mount_sender(const client_t&, mount_map_it mount_it);
  void safe_mount_sender_query(const client_t&, mount_map_it, const query_path_t&);
  mount_map_cit safe_mount(const client_t&, const query_path_t&, const windows_path_t&);
  void safe_unmount(const client_t&, const query_path_t&);
  void safe_unmount_client(const client_t&);

  mount_map_it safe_mount_windows_path(mount_id_t, const windows_path_t&);

private:
  mount_id_t next_mount_m {1};
  mutable std::shared_timed_mutex mutex_m;

  mount_map_t mount_map_m;
  windows_map_t windows_map_m;
  query_map_t query_map_m;
  client_mounts_t client_mounts_m;
};
