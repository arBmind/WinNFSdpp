#include "windows_error.h"

#include <windows.h>

namespace windows{

win32_exception::win32_exception(std::string msg, std::uint32_t get_last_error_code) :
    std::runtime_error(""), final_msg(msg), error_code(get_last_error_code)
{
    LPSTR tmp = NULL;
    auto format_message_return = FormatMessageA(
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |FORMAT_MESSAGE_ALLOCATE_BUFFER,
                NULL,
                this->error_code,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&tmp,
                0,
                NULL
                );
    this->final_msg += ": Code: \"" +
            std::to_string(this->error_code) + "\""
            + " = \"" + std::string(tmp) + "\"";
    LocalFree(tmp);
}

}
