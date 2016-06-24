#pragma once

#include "winfs.h"

#include "binary/binary.h"

#include <windows.h>
#undef max
#undef min

#include <string>
#include <cstdint>

#include "windows_error.h"

namespace winfs {

  template<typename>
  struct object_t;

  enum class seek_method_t : uint32_t {
    begin = FILE_BEGIN,
    current = FILE_CURRENT,
    end = FILE_END,
  };

  struct file_t {

    static windows::win32_return_t remove(std::wstring& filepath) {
      return windows::win32_return_t(::DeleteFileW(filepath.c_str()));
    }

    static windows::win32_return_t move(std::wstring& from, std::wstring& to) {
      return windows::win32_return_t(::MoveFileW(from.c_str(), to.c_str()));
    }

    bool seek(int64_t offset, seek_method_t method = seek_method_t::begin) {
      LARGE_INTEGER distance;
      distance.QuadPart = offset;
      return ::SetFilePointerEx(handle_m, distance, nullptr, static_cast<uint32_t>(method));
    }

    bool truncate() {
      return ::SetEndOfFile(handle_m);
    }

    bool write(const binary_t& binary) {
      if (binary.empty()) return true;
      DWORD writtenBytes;
      bool success = ::WriteFile(
            handle_m, // hFile,
            &binary[0], binary.size(), // Buffer
          &writtenBytes, // NumberOfBytesWritten
          nullptr // Overlapped
          );
      return success;
    }

    bool read(binary_t& binary) {
      if (binary.empty()) return true;
      DWORD readBytes;
      bool success = ::ReadFile(
            handle_m, // hFile,
            &binary[0], binary.size(), // Buffer
          &readBytes, // NumberOfBytesRead
          nullptr // Overlapped
          );
      binary.resize(readBytes);
      return success;
    }

  private:
    template<typename>
    friend struct object_t;

    explicit file_t(const HANDLE& handle) : handle_m(handle) {}

    HANDLE handle_m;
  };

} // namespace winfs
