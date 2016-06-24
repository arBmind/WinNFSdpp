#pragma once

#include "winfs.h"

#include <windows.h>
#undef max
#undef min
#include <cstdint>
#include <string>

#include "windows_error.h"

namespace winfs {

  struct directory_entry_t {
    explicit directory_entry_t(const uint8_t* it)
      : info_m(reinterpret_cast<const FILE_ID_EXTD_DIR_INFO*>(&*it))
    {}

    file_id_t id() const {
      return info_m->FileId;
    }

    std::wstring filename() const {
      return std::wstring(info_m->FileName, info_m->FileNameLength >> 1);
    }
    bool relative() const {
      auto fileName = info_m->FileName;
      return fileName[0] == '.' && (fileName[1] == 0 || (fileName[1] == '.' && fileName[2] == 0));
    }

    uint64_t size() const { return info_m->EndOfFile.QuadPart; }
    uint64_t allocated_size() const { return info_m->AllocationSize.QuadPart; }

    uint32_t attributes() const { return info_m->FileAttributes; }
    bool hidden()    const { return 0 != (attributes() & FILE_ATTRIBUTE_HIDDEN); }
    bool system()    const { return 0 != (attributes() & FILE_ATTRIBUTE_SYSTEM); }
    bool directory() const { return 0 != (attributes() & FILE_ATTRIBUTE_DIRECTORY); }

    bool reparse_point() const { return 0 != (attributes() & FILE_ATTRIBUTE_REPARSE_POINT); }
    bool symlink() const { return reparse_point() && info_m->ReparsePointTag == IO_REPARSE_TAG_SYMLINK; }

    LARGE_INTEGER creationTime() const { return info_m->CreationTime; }
    LARGE_INTEGER lastAccessTime() const { return info_m->LastAccessTime; }
    LARGE_INTEGER lastWriteTime() const { return info_m->LastWriteTime; }
    LARGE_INTEGER changeTime() const { return info_m->ChangeTime; }

    uint32_t next_offset() const { return info_m->NextEntryOffset; }

  private:
    const FILE_ID_EXTD_DIR_INFO* info_m;
  };

  template<typename>
  struct object_t;

  struct directory_t {

    static windows::win32_return_t create(const std::wstring& path) {
      return windows::win32_return_t(::CreateDirectoryW(
                                         path.c_str(), // FileName
                                         nullptr // SecurityAttributes
                                         ));
    }

    static bool remove(const std::wstring& path) {
      return ::RemoveDirectoryW(path.c_str());
    }

    template<size_t buffer_size = 0x10000, typename callback_t> // bool (directory_entry_t)
    bool enumerate(const callback_t& callback) const {
      uint8_t buffer[buffer_size];

      auto result = ::GetFileInformationByHandleEx(
            handle_m,
            FileIdExtdDirectoryRestartInfo,
            buffer, buffer_size
            );
      if ( 0 == result) return false;
      while (true) {
          auto it = (uint8_t*)buffer;
          do {
              auto dir_info = directory_entry_t(it);

              auto more = callback(dir_info);
              if ( !more) return true;

              auto offset = dir_info.next_offset();
              if (0 == offset) break;
              it += offset;
            } while (true);

          result = ::GetFileInformationByHandleEx(
                handle_m,
                FileIdExtdDirectoryInfo,
                buffer, buffer_size
                );
          if (0 == result) break;
        }
      return true;
    }

  private:
    template<typename>
    friend struct object_t;

    explicit directory_t(const HANDLE& handle) : handle_m(handle) {}

    const HANDLE& handle_m;
  };

} // namespace winfs
