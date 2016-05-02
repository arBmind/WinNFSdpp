#include "mount_cache.h"

#include "binary/binary_builder.h"
#include "binary/binary_reader.h"

#include <algorithm>

binary_t
mount_cache_t::safe_save() const
{
  binary_builder_t builder;

  builder.append32(mount_map_m.size());

  for (auto it = mount_map_m.begin(), end = mount_map_m.end(); it != end; ++it) {
      builder.append64(it->first);
      auto& entry = it->second;
      builder.append32(entry.windows_path.length());
      builder.append_binary(entry.windows_path);
    }

  builder.append32(query_map_m.size());
  for (auto it = query_map_m.begin(), end = query_map_m.end(); it != end; ++it) {
      auto& query_path = it->first;
      builder.append32(query_path.length());
      builder.append_binary(query_path);
      auto& mount_it = it->second;
      builder.append64(mount_it->first);
    }

  builder.append32(client_mounts_m.size());
  for (auto it = client_mounts_m.begin(), end = client_mounts_m.end(); it != end; ++it) {
      auto& client = it->first;
      builder.append32(client.length());
      builder.append_binary(client);
      auto& mount_set = it->second;
      builder.append32(mount_set.size());
      for (auto mount : mount_set) {
          builder.append64(mount->first);
        }
    }

  return builder.build();
}

void
mount_cache_t::safe_restore(const binary_t & binary)
{
  auto reader = binary_reader_t::binary(binary);
  auto offset = 0u;

  auto windows_path_count = reader.get32(offset); offset += 4;
  for (auto i = 0u; i < windows_path_count; ++i) {
      auto mount_id = reader.get64(offset); offset += 8;
      auto windows_path_length = reader.get32(offset); offset += 4;
      auto windows_path = reader.get_wstring(offset, windows_path_length);
      offset += windows_path_length * sizeof(wchar_t);

      safe_mount_windows_path(mount_id, windows_path);

      next_mount_m = std::max(mount_id + 1, next_mount_m);
    }

  auto query_path_count = reader.get32(offset); offset += 4;
  for (auto i = 0u; i < query_path_count; ++i) {
      auto query_path_length = reader.get32(offset); offset += 4;
      auto query_path = reader.get_string(offset, query_path_length);
      offset += query_path_length;
      auto mount_id = reader.get64(offset); offset += 8;

      auto mount_it = mount_map_m.find(mount_id);
      if (mount_it == mount_map_m.end()) continue;

      query_map_m[query_path] = mount_it;
    }

  auto client_count = reader.get32(offset); offset += 4;
  for (auto i = 0u; i < client_count; ++i) {
      auto client_length = reader.get32(offset); offset += 4;
      auto client = reader.get_string(offset, client_length);
      offset += client_length;

      auto client_mount_count = reader.get32(offset); offset += 4;
      for (auto j = 0u; j < client_mount_count; ++j) {
          auto mount_id = reader.get64(offset); offset += 8;

          auto mount_it = mount_map_m.find(mount_id);
          if (mount_it == mount_map_m.end()) continue;

          safe_mount_sender(client, mount_it);
        }
    }
}

void
mount_cache_t::safe_mount_sender(const client_t& client, mount_map_it mount_it) {
  entry_t& entry = mount_it->second;
  auto entry_client_it = entry.clients.find(client);
  if (entry_client_it != entry.clients.end()) return; // already mounted
  auto client_it = client_mounts_m.find(client);
  if (client_it == client_mounts_m.end()) {
      auto tmp = client_mounts_m.insert(std::make_pair(client, mounts_set_t()));
      assert(tmp.second); // error
      client_it = tmp.first;
    }
  mounts_set_t& client_mounts = client_it->second;
  client_mounts.insert(mount_it);
  entry.clients.insert(client_view_t(client_it->first));
}

void
mount_cache_t::safe_mount_sender_query(const client_t& client, mount_map_it mount_it, const query_path_t& query_path) {
  query_map_m[query_path] = mount_it;
  safe_mount_sender(client, mount_it);
}

mount_cache_t::mount_map_cit
mount_cache_t::safe_mount(const client_t& client, const query_path_t& query_path, const windows_path_t& windows_path) {
  mount_id_t mount_id = next_mount_m++;
  auto mount_it = safe_mount_windows_path(mount_id, windows_path);

  if (mount_it != mount_map_m.end()) {
      safe_mount_sender_query(client, mount_it, query_path);
    }
  return mount_it;
}

void
mount_cache_t::safe_unmount(const client_t& client, const query_path_t& query_path) {
  // TODO
}

void
mount_cache_t::safe_unmount_client(const client_t& client) {
  // TODO
}

mount_cache_t::mount_map_it
mount_cache_t::safe_mount_windows_path(mount_id_t mount_id, const windows_path_t& windows_path)
{
  entry_t entry;
  entry.directory = winfs::open_path(windows_path);
  if (!entry.directory.valid()) return mount_map_m.end();
  entry.windows_path = entry.directory.fullpath();
  auto& filehandle = mount_filehandle_t::create_in_binary(entry.filehandle);
  filehandle.mount_id = mount_id;
  entry.directory.id(filehandle.volume_file_id);

  auto tmp = mount_map_m.insert(std::make_pair(mount_id, std::move(entry)));
  if (tmp.second) {
      auto mount_it = tmp.first;
      entry_t& entry_ref = mount_it->second;
      windows_map_m.insert(std::make_pair(windows_path_view_t(entry_ref.windows_path), mount_it));

      return tmp.first;
    }
  return mount_map_m.end();
}
