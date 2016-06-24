/*! @file windows_error.h
 * Utility enums and classes for handling Windows / Win32 Errors
 * */

#pragma once

#include <cstdint>
#include <exception>
#include <string>
#include <utility>
#include <functional>

namespace windows{
using win32_error_code_t = uint32_t; //*< Aka DWORD

/*! @brief Exception class for the windows API aka WIN32 functions which are using GetLastError
 * @notice Handles the inconsistent behaveiour of successfull but error setting calls
*/
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

/*! @brief Utility class to pass the complete result of a win32 call
 * @todo When C++17 usable change super class to public std::pair<bool, std::optional> and exception in get_error to std::bad_optional_access
 * */
class win32_return_t : private std::pair<bool, win32_error_code_t> {
public:
    /*! @brief to manually inidcate a successfull call
     * */
    constexpr win32_return_t () : std::pair<bool, win32_error_code_t> (true,0) {}

    win32_return_t (bool func_return, win32_error_code_t get_last_error_return);
    /*! @brief Will execute the given win32_call, and store the results accordingly
     * @attention Will not store an error if the win32 call returned true
     * @param win32_call Callback to a win32 bool returning function, which will
     * return false if there is an error which will be retrived by GetLastError
     * */
    win32_return_t (std::function<bool (void)> win32_call);

   /*! @brief Will execute the given win32 call, and store a GetLastError result if it didn't returned the excepted value
    * @param win32_call Callback to a win32 which should return @ref expected
    * @param expected Expected value from win32 routine
    * */

    /*! If the return value of the given win32_routine does not match the criteria (per default is equal), get error accordingly
     * */
    template <typename ReturnType, typename Comp = std::equal_to>
    win32_return_t (std::function<ReturnType (void)> win32_call, ReturnType expected)
        : win32_return_t(Comp(win32_call(),expected))
    {
    }

    /*! @brief To determine whatever win32 call was successfull or not
     * */
    explicit operator bool() const{
        return this->first;
    }
    bool operator!() const{
        return !this->first;
    }
    /*! @brief Returns the error code which was retrieved after the win32 call or throws an exception if no error was set
     * @throws std::logic_error
     * */
    win32_error_code_t get_error() const{
        if (first){
            throw std::runtime_error("No error was set");
        }
        return this->second;
    }
    std::string to_string() const;

};

uint32_t to_nfs3_error(win32_error_code_t win32_error);
}

