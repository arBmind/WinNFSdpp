#pragma once

//#include <winternl.h>

//using _NtCreateFile = NTSTATUS(NTAPI *)(
//OUT PHANDLE FileHandle,
//IN ACCESS_MASK DesiredAccess,
//IN POBJECT_ATTRIBUTES ObjectAttributes,
//OUT PIO_STATUS_BLOCK IoStatusBlock,
//IN PLARGE_INTEGER AllocationSize OPTIONAL,
//IN ULONG FileAttributes,
//IN ULONG ShareAccess,
//IN ULONG CreateDisposition,
//IN ULONG CreateOptions,
//IN PVOID EaBuffer OPTIONAL,
//IN ULONG EaLength
//);
//using _NtOpenFile = NTSTATUS(NTAPI *)(
//OUT PHANDLE FileHandle,
//IN ACCESS_MASK DesiredAccess,
//IN POBJECT_ATTRIBUTES ObjectAttributes,
//OUT PIO_STATUS_BLOCK IoStatusBlock,
//IN ULONG ShareAccess,
//IN ULONG OpenOptions
//);

//using _RtlInitUnicodeString = VOID(NTAPI *)(
//PUNICODE_STRING DestinationString,
//PCWSTR SourceString
//);

//TEST(winternl, ntCreateFile) {
//  auto ntdllHandle = GetModuleHandle(L"ntdll.dll");
//  _NtOpenFile pNtOpenFile = (_NtOpenFile)GetProcAddress(ntdllHandle, "NtOpenFile");
//  _NtCreateFile pNtCreateFile = (_NtCreateFile)GetProcAddress(ntdllHandle, "NtCreateFile");
//  _RtlInitUnicodeString pRtlInitUnicodeString = (_RtlInitUnicodeString)GetProcAddress(ntdllHandle, "RtlInitUnicodeString");

//  UNICODE_STRING unicodeString;
//  pRtlInitUnicodeString(&unicodeString, L"\\??\\C:\\testfile.txt");

//  OBJECT_ATTRIBUTES objAttribs;
//  objAttribs.Length = sizeof(OBJECT_ATTRIBUTES);
//  //objAttribs.RootDirectory = INVALID_HANDLE_VALUE;
//  objAttribs.ObjectName = &unicodeString;
//  objAttribs.Attributes = OBJ_CASE_INSENSITIVE;
//  //objAttribs.SecurityDescriptor = nullptr;
//  //objAttribs.SecurityQualityOfService = nullptr;

//  HANDLE hFile = INVALID_HANDLE_VALUE;
//  IO_STATUS_BLOCK ioStatusBlock;
//  memset(&ioStatusBlock, 0, sizeof(ioStatusBlock));

//  LARGE_INTEGER largeInteger;
//  largeInteger.QuadPart = 2048;

//  auto out = pNtCreateFile(&hFile, FILE_GENERIC_WRITE, &objAttribs, &ioStatusBlock, &largeInteger,
//                  FILE_ATTRIBUTE_NORMAL, 0, FILE_CREATE, 0, NULL, NULL);

//  IoQueryFileDosDeviceName

////  auto out = pNtCreateFile(
////        &hFile, // FileHandle
////        0, // DesiredAccess
////        &objAttribs, // ObjectAttributes
////        &ioStatusBlock,
////        nullptr, // AllocationSize
////        0, // FileAttributes
////        winfs::FILE_SHARE_ALL, // ShareAccess
////        OPEN_EXISTING, // CreationDisposition
////        0, // CreateOptions
////        nullptr, 0 // EABuffer
////        );
//  EXPECT_TRUE(NT_SUCCESS(out));
//  if (hFile != INVALID_HANDLE_VALUE) {
//      CloseHandle(hFile);
//    }
//}

//#include <windows.h>
//#include <string>
//#include <gtest/gtest.h>

//TEST(object, fullPathname) {

//  auto str = L"Z:\\vagrant-cache/ubuntu/../trusty64";

//  std::wstring buffer;
//  buffer.resize(0x10000);

//  auto size = GetFullPathNameW(str, buffer.size() - 1, (wchar_t*)buffer.c_str(), nullptr);

//  EXPECT_NE(size, 0);
//  buffer.resize(size);
//  std::wcout << "text: " << buffer << std::endl;
//}

//TEST(object, device_fullpath) {
//  auto handle = winfs::unique_object_t::by_path(L"\\\\?\\\\Device\\TrueCryptVolumeD}");
//  EXPECT_TRUE(handle.valid());
//  auto fullpath = handle.fullpath();
//  EXPECT_EQ(L"\\Device\\TrueCryptVolumeD\\arB\\Ansible\\dresden-weekly\\rails-example", fullpath);
//}

#include <windows.h>
#include <string>
#include <gtest/gtest.h>
#include <iostream>

TEST(volume, pathName) {

  std::wstring buffer;
  buffer.resize(0x1000);

  auto success = ::GetVolumePathNameW(L"Z:\\Temp", (wchar_t*)buffer.c_str(), buffer.size());
  if (!success) {
      std::cout << "error " << GetLastError() << std::endl;
    }

  buffer.resize(wcslen(buffer.c_str()));

  std::wcout << "PathName " << buffer << std::endl;
}

TEST(volume, dosdevice) {

  std::wstring buffer;
  buffer.resize(0x10000);

  auto size = ::QueryDosDeviceW(nullptr, (wchar_t*)buffer.c_str(), buffer.size());
  if (0 == size) {
      std::cout << "error " << GetLastError() << std::endl;
      return;
    }

  size_t i = 0;
  while (i < size - 1) {
      auto len = wcslen(buffer.c_str()+i);

      if (len == 2)
      std::wcout << buffer.c_str()+i << std::endl;

      i += len + 1;
    }
}

TEST(volume, logical_drive_strings) {

  std::wstring buffer;
  buffer.resize(0x1000);

  auto size = ::GetLogicalDriveStringsW(buffer.size(), (wchar_t*)buffer.c_str());
  if (0 == size) {
      std::cout << "error " << GetLastError() << std::endl;
      return;
    }

  size_t i = 0;
  while (i < size - 1) {
      auto len = wcslen(buffer.c_str()+i);

      std::wcout << buffer.c_str()+i << std::endl;

      i += len + 1;
    }
}

TEST(volume, scanning) {

  struct volume_t {
    ~volume_t() {
      if (valid()) ::FindVolumeClose(handle_m);
    }

    bool valid() const { return handle_m != INVALID_HANDLE_VALUE; }

    static volume_t start(std::wstring& str) {
      volume_t result;
      str.resize(str.capacity());
      result.handle_m = ::FindFirstVolumeW(const_cast<wchar_t*>(str.c_str()), str.size() - 1);
      str.resize(wcslen(str.c_str()));
      return result;
    }

    bool next(std::wstring& str) {
      str.resize(str.capacity());
      auto result = ::FindNextVolumeW(handle_m, const_cast<wchar_t*>(str.c_str()), str.size() - 1);
      str.resize(wcslen(str.c_str()));
      return result;
    }

  private:
    HANDLE handle_m = INVALID_HANDLE_VALUE;
  };
  struct mount_t {
    ~mount_t() {
      if (valid()) ::FindVolumeMountPointClose(handle_m);
    }

    bool valid() const { return handle_m != INVALID_HANDLE_VALUE; }

    static mount_t start(const std::wstring& volume, std::wstring& str) {
      mount_t result;
      str.resize(str.capacity());
      result.handle_m = ::FindFirstVolumeMountPointW(volume.c_str(), const_cast<wchar_t*>(str.c_str()), str.size() - 1);
      str.resize(wcslen(str.c_str()));
      return result;
    }

    bool next(std::wstring& str) {
      str.resize(str.capacity());
      auto result = ::FindNextVolumeMountPointW(handle_m, const_cast<wchar_t*>(str.c_str()), str.size() - 1);
      str.resize(wcslen(str.c_str()));
      return result;
    }

  private:
    HANDLE handle_m = INVALID_HANDLE_VALUE;
  };

  std::wstring volume_name;
  const int buffer_size = 0x10000;
  volume_name.reserve(buffer_size);
  auto volume = volume_t::start(volume_name);
  if (!volume.valid()) return;

  do {
      std::wcout << volume_name << std::endl;

      std::wstring mount_path;
      mount_path.resize(buffer_size);

      DWORD mount_size = 0;
      auto success = ::GetVolumePathNamesForVolumeNameW(volume_name.c_str(), (wchar_t*)mount_path.c_str(), mount_path.size(), &mount_size);
      if (!success) {
          std::cout << "error: " << GetLastError() << std::endl;
        }
      else if (1 >= mount_size) {
          std::wcout << "not mounted" << std::endl;
        }
      else {
          size_t i = 0;
          while (i < mount_size - 1) {
              auto len = wcslen(mount_path.c_str()+i);
              std::wcout << mount_path.c_str()+i << std::endl;
              i += len + 1;
            }
        }

//      auto mount = mount_t::start(volume_name, mount_path);
//      if (!mount.valid()) continue;

//      do {
//          std::wcout << mount_path << std::endl;
//        } while (mount.next(mount_path));

    } while (volume.next(volume_name));
}

TEST(volume, nameFromMountPoint) {
  //  auto str = L"Z:\\"; // Does not work for RamDisk
  auto str = L"C:\\";

  std::wstring buffer;
  buffer.resize(0x10000);

  auto success = GetVolumeNameForVolumeMountPointW(str, (wchar_t*)buffer.c_str(), buffer.size() - 1);
  if (!success) {
      std::cout << "error: " << GetLastError() << std::endl;
    }
  buffer.resize(wcslen(buffer.c_str()));

  std::wcout << "name: " << buffer << std::endl;
}
