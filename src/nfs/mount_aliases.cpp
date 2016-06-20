#include "mount_aliases.h"

#include "container/string_convert.h"
#include "winfs/winfs_object.h"

#include <iostream>
#define GOOGLE_GLOG_DLL_DECL
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>

mount_aliases_t::windows_path_t
mount_aliases_t::alias_subpath_to_windows(const alias_path_t& alias_path) {
  windows_path_t result = convert::to_wstring(alias_path);
  std::transform(result.begin(), result.end(), result.begin(), [](wchar_t chr) {
      if (chr == L'/') return L'\\';
      return chr;
    });
  return result;
}

mount_aliases_t::alias_path_t
mount_aliases_t::windows_to_alias_path(const windows_path_t &windows_path)
{
  alias_path_t result;

  // remove the prefix
  static wchar_t prefix[] = L"\\\\?\\";
  size_t offset = 0;
  if (windows_path.length() >= 4
      && windows_path[0] == prefix[0]
      && windows_path[1] == prefix[1]
      && windows_path[2] == prefix[2]
      && windows_path[3] == prefix[3]) offset = 4;

  // convert to uft8
  result = convert::to_string_with_offset(windows_path, offset);

  // convert \ to /
  std::transform(result.begin(), result.end(), result.begin(), [](char chr) {
      if (chr == '\\') return '/';
      return chr;
    });

  // convert C: to /C
  if (result.size() >= 2 && result[1] == ':') {
      result[1] = result[0];
      result[0] = '/';
    }
  // ensure start with /
  else if (result.size() >= 1 && result[0] != '/') {
      result.insert(result.begin(), '/');
    }
  // ensure no tailing slash
  while (result.size() >= 1 && result.back() == '/') result.resize(result.size() - 1);
  return result;
}

mount_aliases_t::windows_path_t
mount_aliases_t::by_alias_path_safe(const alias_path_t& alias_path) const {
  auto alias_path_length = alias_path.length();
  auto end_it = store_m.end();
  auto best_it = end_it;
  auto best_length = 0u;

  for (auto it = store_m.begin(); it != end_it; ++it) {
      const entry_t& entry = *it;
      auto entry_length = entry.alias_path.length();

      if (entry_length > alias_path_length) continue; // entry path is longer
      if (entry_length < best_length) continue; // not better
      if (entry_length == alias_path_length) {
          if (entry.alias_path != alias_path) continue; // not equal match
        }
      else {
          assert(entry_length < alias_path_length);
          if (0 != alias_path.compare(0, entry_length, entry.alias_path)) continue; // does not start with entry
          if (alias_path[entry_length] != '/') continue; // not a full folder
        }
      best_it = it;
      best_length = entry_length;
    }

  if (best_it == end_it) {
      return {}; // not found
    }
  const entry_t& best_entry = *best_it;
  auto subpath = alias_subpath_to_windows(alias_path.substr(best_length));
  return best_entry.windows_path + subpath;
}

bool
mount_aliases_t::add_safe(source_t source, const windows_path_t& windows_path, const alias_path_t& alias_path) {
  if (std::any_of(store_m.begin(), store_m.end(), [&](const entry_t& entry) {
                  return entry.alias_path == alias_path;
})) return false;

  auto directory = winfs::open_path<0, FILE_FLAG_BACKUP_SEMANTICS>(windows_path);
  if ( !directory.valid()) return false;

  entry_t entry;
  entry.windows_path = directory.fullpath();
  entry.alias_path = alias_path.empty() ? windows_to_alias_path(windows_path) : alias_path;
  entry.source = source;
  store_m.push_back(entry);
  LOG(INFO) << "Alias by " << std::to_string(entry.source) << " for " << convert::to_string(entry.windows_path)
            << " at " << entry.alias_path;
  return true;
}

void
mount_aliases_t::set_aliases_safe(source_t source, const alias_vector_t& alias_vector) {
  // remove old mounts
  for (auto it = store_m.begin(); it != store_m.end();) {
      const entry_t& entry = *it;
      if (entry.source == source) {
          it = store_m.erase(it);
        }
      else
        ++it;
    }

  // add new mounts
  for (const auto& pair : alias_vector) {
      add_safe(source, pair.first, pair.second);
    }
}
