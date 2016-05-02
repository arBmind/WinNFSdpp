#pragma once

#include <windows.h>
#undef max
#undef min

namespace winfs {

  using file_id_t = FILE_ID_128;
  using volume_id_t = ULONGLONG;
  using volume_file_id_t = FILE_ID_INFO;

} // namespace winfs
