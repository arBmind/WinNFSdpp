/*! @file windows_error.h
 * Utility enums and classes for handling Windows / Win32 Errors
 * */

#pragma once

#include <cstdint>
#include <exception>
#include <string>
#include <utility>

namespace windows{
using win32_error_code_t = uint32_t; //*< Aka DWORD

/*! @brief Exception class for the windows API aka WIN32 functions which are using GetLastError*/
class win32_exception : public std::runtime_error{
public:
    /*! @param msg User supplied message which will lead the full exception text
     * @param get_last_error_code Error code from GetLastError
     */
    explicit win32_exception(std::string msg, std::uint32_t get_last_error_code);
    /*! @return The user supplied message + the error code + a string description of the error code
     * */
    const char* what() const override {
        return final_msg.c_str();
    }
    /*! @returns The Win32 specific error code*/
    win32_error_code_t code () const {
        return this->error_code;
    }
    virtual ~win32_exception(){}
private:
    std::string final_msg;
    win32_error_code_t error_code;
};

struct win32_return_t : std::pair<bool, win32_error_code_t> {
    win32_return_t (bool func_return);
    win32_return_t (bool func_return, win32_error_code_t get_last_error_return);

};

}

