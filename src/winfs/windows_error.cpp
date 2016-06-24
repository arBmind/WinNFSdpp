#include "windows_error.h"

#include <windows.h>

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

win32_return_t::win32_return_t (bool returned_as_bool) {
    this->first = returned_as_bool;
    if (!this->first)
        this->second = GetLastError();
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


}
