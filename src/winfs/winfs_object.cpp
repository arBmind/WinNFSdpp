#include "winfs_object.h"

namespace winfs {

//  bool volume_info() {
//    std::wstring volume_name;
//    volume_name.resize(MAX_PATH+1);

//    DWORD serial_number;
//    DWORD max_component_length;
//    DWORD flags;

//    std::wstring fs_name;
//    fs_name.resize(MAX_PATH+1);

//    bool success = ::GetVolumeInformationByHandleW(
//          handle_m, // File
//          &volume_name[0], volume_name.size() - 1, // VolumeName
//          &serial_number, // VolumeSerialNumber
//          &max_component_length, // MaximumComponentLength
//          &flags, // FileSystemFlags
//          &fs_name[0], fs_name.size() - 1 // FileSystemName
//          );
//    if (!success) {
//        volume_name.clear();
//        fs_name.clear();
//        return false;
//      }
//    volume_name.resize(wcslen(&volume_name[0]));
//    fs_name.resize(wcslen(&fs_name[0]));

//    std::wcout << volume_name << " - " << fs_name << std::endl;
//    std::cout << std::hex << serial_number << " - " << std::dec << max_component_length << " - " << std::hex << flags << std::dec << std::endl;

//    return true;
//  }

} // namespace winfs
