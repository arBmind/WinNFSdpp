#include "windows_error.h"

#include <windows.h>
#include "nfs/nfs3.h"

namespace windows{

namespace{
std::string& win32_code_to_text(win32_error_code_t error_code, std::string &string){
    LPSTR tmp = NULL;
    auto format_message_return = FormatMessageA(
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |FORMAT_MESSAGE_ALLOCATE_BUFFER,
                NULL,
                error_code,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&tmp,
                0,
                NULL
                );
    if (format_message_return == 0){
        throw std::runtime_error("FormatMessageA failed in win32_code_to_text");
    }
    string += ": Code: \"" +
            std::to_string(error_code) + "\""
            + " = \"" + std::string(tmp) + "\"";
    LocalFree(tmp);
    return string;
}
}

win32_return_t::win32_return_t (std::function<bool (void)> win32_call)
{
    this->first = win32_call();
    if (!first){
        this->second = GetLastError();
    }
}


win32_exception::win32_exception(std::string msg, std::uint32_t get_last_error_code) :
    std::runtime_error(""), final_msg(msg), error_code(get_last_error_code)
{
    win32_code_to_text(this->error_code, this->final_msg);
}

std::string win32_return_t::to_string() const{
    std::string msg("Function returned ");
    msg += (first ? "successfull" : " unsucessfull");

    if (!first){
        msg += " and set GetLastError to" ;
        win32_code_to_text(second,msg);
    }
    return msg;
}

uint32_t to_nfs3_error(win32_error_code_t win32_error){
    nfs3::status_t nfs_error;
    switch (win32_error){
    case ERROR_SUCCESS:
        nfs_error = nfs3::status_t::OK;
        break;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        nfs_error = nfs3::status_t::ERR_NO_ENTRY;
        break;
    case ERROR_TOO_MANY_OPEN_FILES:
    case ERROR_WRITE_FAULT:
    case ERROR_READ_FAULT:
        nfs_error = nfs3::status_t::ERR_IO;
        break;
    case ERROR_INVALID_HANDLE:
        nfs_error = nfs3::status_t::ERR_STALE;
        break;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
    case ERROR_HANDLE_DISK_FULL:
    case ERROR_DISK_FULL:
        nfs_error = nfs3::status_t::ERR_NOSPC;
       break;
    case ERROR_INVALID_DRIVE:
    case ERROR_DEV_NOT_EXIST:
        nfs_error = nfs3::status_t::ERR_NODEV;
        break;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        nfs_error = nfs3::status_t::ERR_EXIST;
        break;
    case ERROR_ACCESS_DENIED:
        nfs_error = nfs3::status_t::ERR_PERM;
        break;
    case ERROR_INVALID_ACCESS:
        nfs_error = nfs3::status_t::ERR_ACCESS;
        break;
    default:
        nfs_error = nfs3::status_t::ERR_SERVERFAULT;
        break;
    }
    return static_cast<uint32_t> (nfs_error);
}

}
