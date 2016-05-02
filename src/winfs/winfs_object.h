#pragma once

#include "winfs.h"
#include "winfs_file.h"
#include "winfs_directory.h"

#include "windows_handle.h"

#include <string>

#include <cassert>
#include <iostream>

#include "GSL/string_span.h"

namespace winfs {

  template<typename>
  struct object_t;

  using shared_object_t = object_t<windows::shared_handle_t>;
  using unique_object_t = object_t<windows::unique_handle_t>;

  const auto FILE_SHARE_ALL = FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE;

  template<uint32_t desiredAccess = 0, uint32_t flags = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS>
  unique_object_t open_path(const std::wstring& path);

  template<uint32_t desiredAccess = 0, uint32_t flags = FILE_ATTRIBUTE_NORMAL>
  unique_object_t create_file(const std::wstring& path);

  template<typename base_handle_t>
  struct object_t : base_handle_t {

    template<uint32_t desiredAccess = 0,
             uint32_t flags = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
             uint32_t shareMode = FILE_SHARE_ALL>
    unique_object_t by_id(const file_id_t& id) const {
      unique_object_t result;
      FILE_ID_DESCRIPTOR descriptor;
      descriptor.dwSize = sizeof(descriptor);
      descriptor.Type = ExtendedFileIdType;
      descriptor.ExtendedFileId = id;
      ((object_t*)&result)->handle_m = ::OpenFileById(
            this->handle_m, // VolumeHint
            &descriptor,// FileId
            desiredAccess, // DesiredAccess
            shareMode, // ShareMode
            nullptr, // SecurityAttributes
            flags // FlagsAndAttributes
            );
      return result;
    }

    template<uint32_t desiredAccess, uint32_t flags = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS>
    unique_object_t reopen_for() const {
      unique_object_t result;
      result.handle_m = ::ReOpenFile(
            this->handle_m, // OriginalFile
            desiredAccess, // DesiredAccess
            FILE_SHARE_ALL, // ShareMode
            flags // FlagsAndAttributes
            );
      return result;
    }

    template<uint32_t desiredAccess = 0, uint32_t flags = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS>
    unique_object_t lookup(const std::wstring& name) {
      auto base_path = fullpath();
      auto path = base_path + L'\\' + name;
      return open_path<desiredAccess, flags>(path);
    }

    bool id(volume_file_id_t& volume_file_id) const {
      memset(&volume_file_id, 0, sizeof(volume_file_id));
      return ::GetFileInformationByHandleEx(
            this->handle_m,
            FileIdInfo,
            &volume_file_id, sizeof(volume_file_id)
            );
    }

    file_t as_file() const { return file_t(this->handle_m); }
    directory_t as_directory() const { return directory_t(this->handle_m); }

    template<size_t buffer_size = 0x10000>
    std::wstring path() const {
      std::wstring result;
      uint8_t buffer[buffer_size];

      auto success = ::GetFileInformationByHandleEx(
            this->handle_m,
            FileNameInfo,
            buffer, buffer_size - 1
            );
      if ( !success) return result;
      auto info = reinterpret_cast<const FILE_NAME_INFO*>(buffer);
      result = std::wstring(info->FileName, info->FileNameLength >> 1);
      return result;
    }

    template<size_t buffer_size = 0x10000>
    std::wstring fullpath() const {
      assert(this->valid());
      std::wstring result;
      uint8_t buffer[buffer_size];

      auto size = ::GetFinalPathNameByHandleW(
            this->handle_m,
            (wchar_t*)buffer, buffer_size - 1,
            FILE_NAME_NORMALIZED | VOLUME_NAME_DOS // Flags
            );
//      if (0 == size && this->valid()) {
//          size = ::GetFinalPathNameByHandleW(
//                this->handle_m,
//                (wchar_t*)buffer, buffer_size - 1,
//                FILE_NAME_NORMALIZED | VOLUME_NAME_NT // Flags
//                );
//          if (0 == size) return result; // sth is wrong

//          struct search_t {
//            ~search_t() {
//              if (valid()) ::FindVolumeClose(handle_m);
//            }

//            bool valid() const { return handle_m != INVALID_HANDLE_VALUE; }

//            static search_t start(std::wstring& str) {
//              search_t result;
//              str.resize(str.capacity);
//              result.handle_m = ::FindFirstVolumeW(const_cast<wchar_t*>(str.c_str()), str.size() - 1);
//              str.resize(strlen(str.c_str()));
//              return result;
//            }

//            static bool next(std::wstring& str) {
//              str.resize(str.capacity);
//              auto result = ::FindNextVolumeW(handle_m, const_cast<wchar_t*>(str.c_str()), str.size() - 1);
//              str.resize(strlen(str.c_str()));
//              return result;
//            }

//          private:
//            HANDLE handle_m = INVALID_HANDLE_VALUE;
//          };



//          result.reserve(buffer_size);
//          auto search = search_t::start(result);
//          if (!search.valid()) return result;

//          ::FindFirstVolumeMountPointA();
//        }
      auto ptr = reinterpret_cast<const std::wstring::value_type*>(buffer);
      result = std::wstring(ptr, size);
      return result;
    }

    bool basic_info(FILE_BASIC_INFO& basic_info) const {
      memset(&basic_info, 0, sizeof(basic_info));
      return ::GetFileInformationByHandleEx(
            this->handle_m,
            FileBasicInfo,
            &basic_info, sizeof(basic_info)
            );
    }

    bool standard_info(FILE_STANDARD_INFO& standard_info) const {
      memset(&standard_info, 0, sizeof(standard_info));
      return ::GetFileInformationByHandleEx(
            this->handle_m,
            FileStandardInfo,
            &standard_info, sizeof(standard_info)
            );
    }

    bool set_size(uint64_t size) {
      FILE_END_OF_FILE_INFO end_of_file_info;
      end_of_file_info.EndOfFile.QuadPart = size;

      auto result = SetFileInformationByHandle(
            this->handle_m, // File
            FileEndOfFileInfo, // FileInformationClass
            &end_of_file_info, sizeof end_of_file_info // FileInformation
            );
      return result;
    }

    bool set_basic_info(const FILE_BASIC_INFO& basic_info) const {
      return ::SetFileInformationByHandle(
            this->handle_m,
            FileBasicInfo,
            (void*)&basic_info, sizeof basic_info
            );
    }

  private:
    friend std::wstring resolve_symlink(const std::wstring&);

    template<uint32_t, uint32_t>
    friend unique_object_t open_path(const std::wstring&);

    template<uint32_t, uint32_t>
    friend unique_object_t create_file(const std::wstring&);
  };

  template<uint32_t desiredAccess, uint32_t flags>
  inline unique_object_t open_path(const std::wstring& path) {
    unique_object_t result;
    result.handle_m = ::CreateFileW(
          path.c_str(), // FileName
          desiredAccess, // DesiredAccess
          FILE_SHARE_ALL, // ShareMode
          nullptr, // SecurityAttributes
          OPEN_EXISTING, // CreationDisposition
          flags, // FlagsAndAttributes
          NULL // TemplateFile
          );
    return result;
  }

  template<uint32_t desiredAccess, uint32_t flags>
  inline unique_object_t create_file(const std::wstring& path) {
    unique_object_t result;
    result.handle_m = ::CreateFileW(
          path.c_str(), // FileName
          desiredAccess, // DesiredAccess
          FILE_SHARE_ALL, // ShareMode
          nullptr, // SecurityAttributes
          CREATE_NEW, // CreationDisposition
          flags, // FlagsAndAttributes
          NULL // TemplateFile
          );
    return result;
  }

  inline std::wstring resolve_symlink(const std::wstring& link_path) {
    std::wstring result;
    auto handle = open_path(link_path);
    if (!handle.valid()) return result;

    BYTE buffer[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
    DWORD bytesReturned;
    auto success = DeviceIoControl(
          handle.handle_m, // Device
          FSCTL_GET_REPARSE_POINT, // ControlCode
          NULL, 0, // InBuffer, Size
          &buffer, sizeof(buffer), // OutBuffer, Size
          &bytesReturned, // BytesReturned
          0 // Overlapped
          );
    if (!success || 0 == bytesReturned) return result;

#ifndef SYMLINK_FLAG_RELATIVE
    constexpr auto SYMLINK_FLAG_RELATIVE = 1;
#endif
    struct REPARSE_DATA_BUFFER {
      ULONG  ReparseTag;
      USHORT ReparseDataLength;
      USHORT Reserved;
      union {
        struct {
          USHORT SubstituteNameOffset;
          USHORT SubstituteNameLength;
          USHORT PrintNameOffset;
          USHORT PrintNameLength;
          ULONG  Flags;
          WCHAR  PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
          USHORT SubstituteNameOffset;
          USHORT SubstituteNameLength;
          USHORT PrintNameOffset;
          USHORT PrintNameLength;
          WCHAR  PathBuffer[1];
        } MountPointReparseBuffer;
        struct {
          UCHAR DataBuffer[1];
        } GenericReparseBuffer;
      };
    };

    const REPARSE_DATA_BUFFER* reparseBuffer = (REPARSE_DATA_BUFFER*)&buffer;
    if (IO_REPARSE_TAG_SYMLINK == reparseBuffer->ReparseTag) {
        result.assign(reparseBuffer->SymbolicLinkReparseBuffer.PathBuffer + reparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameOffset/2,
                      reparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength/2);
        if (gsl::cwstring_span<>(result).first(4) == L"\\??\\") result = result.substr(4);
        if (reparseBuffer->SymbolicLinkReparseBuffer.Flags & SYMLINK_FLAG_RELATIVE) {
            result = link_path + L'/' + result;
          }
        std::wstring buffer;
        buffer.resize(0x10000);
        auto size = GetFullPathNameW(result.c_str(), buffer.size() - 1, (wchar_t*)buffer.c_str(), nullptr);
        buffer.resize(size);
        return buffer;
      }
    if (IO_REPARSE_TAG_MOUNT_POINT == reparseBuffer->ReparseTag) {
        result.assign(reparseBuffer->SymbolicLinkReparseBuffer.PathBuffer + reparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameOffset/2,
                      reparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength/2);
        auto prefix = gsl::cwstring_span<>(result).first(4);
        if (prefix == L"\\??\\" || prefix == L"\\\\?\\") result = result.substr(4);
        // this is always an absolute path
        return result;
      }
    return result;
  }

  inline std::wstring deep_resolve_path(const std::wstring& original_path) {
    {
      auto handle = open_path<0, FILE_FLAG_BACKUP_SEMANTICS>(original_path);
      if (!handle.valid()) return {};
      auto path = handle.fullpath();
      if (!path.empty()) return path;
    }
    auto rbegin = original_path.rbegin();
    auto rend = original_path.rend();
    auto rcursor = rbegin;
    while (rcursor != rend) {
        auto subpath = std::wstring(rend.base(), rcursor.base());

        auto target_subpath = resolve_symlink(subpath);
        if (!target_subpath.empty()) {
            auto target_path = target_subpath.append(rcursor.base(), rbegin.base());
            return deep_resolve_path(target_path);
          }

        rcursor = std::find_if(rcursor, rend, [](wchar_t chr) { return chr == '/' || chr == '\\'; });
        if (rcursor == rend) break;
        rcursor++;
      }
    auto path = original_path;
    std::transform(path.begin(), path.end(), path.begin(), [](wchar_t chr) {
        if (chr == L'/') return L'\\';
        return chr;
      });
    return path;
  }

} // namespace winfs
