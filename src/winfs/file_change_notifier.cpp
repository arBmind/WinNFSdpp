/*! @file file_change_notifier.cpp
 * */
#include "file_change_notifier.h"

#include <algorithm>
#include <Windows.h>

file_change_notifier::
file_change_notifier(full_path_t file_path,
                     std::function<void(full_path_t, change_event)> callback,
                     change_event events) :
    callback_(callback), events_(events), notification_handle_(INVALID_HANDLE_VALUE), continue_waiting_(false)
{
    split_path(file_path,this->directory_path_,this->file_name_);

    DWORD dwNotifyFiler = 0;
    if(events_ & change_event::file_attributes){
        dwNotifyFiler |= FILE_NOTIFY_CHANGE_LAST_WRITE;
        dwNotifyFiler |= FILE_NOTIFY_CHANGE_SECURITY;
        dwNotifyFiler |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
    } if(events_ & change_event::file_name) {
        dwNotifyFiler |= FILE_NOTIFY_CHANGE_FILE_NAME;
    } if(events_ & change_event::file_size) {
        dwNotifyFiler |= FILE_NOTIFY_CHANGE_SIZE;
    }

    this->notification_handle_ = FindFirstChangeNotification(this->directory_path_.c_str(),false,dwNotifyFiler);
    if (notification_handle_ == INVALID_HANDLE_VALUE) {
        throw win32_exception("FindFirstChangeNotification failed", GetLastError());
    }
}

file_change_notifier::~file_change_notifier()  {
    if (!this->continue_waiting_){
        return;
    }
    this->continue_waiting_ = false;
    try{
        this->waiting_future.get();}
    catch (...) {}
    FindCloseChangeNotification(this->notification_handle_);
}

void file_change_notifier::split_path(full_path_t full_path,
                                      internal_path_t& directory_path,
                                      internal_path_t& file_name){
    directory_path.resize(full_path.size()+64, 0);
    TCHAR *file_namepart_ptr;

    auto call_GetFullPathNameW = [&]() {
        return GetFullPathNameW(full_path.c_str(),
                                directory_path.size(),
                                (internal_path_t::value_type*) directory_path.data(),/* Cast won't be needed after C++17 */
                                &file_namepart_ptr);
    };

    DWORD return_value = call_GetFullPathNameW();
    if (return_value >= directory_path.size()){
        directory_path.resize(return_value, 0);
        call_GetFullPathNameW();
    } else if (return_value == 0){
        throw win32_exception("Call of GetFullPathNameW failed with \"",
                              GetLastError());
    }
    file_name = file_namepart_ptr;
    directory_path.resize(return_value - file_name.size());
}

void file_change_notifier::start_watching(){
    if (this->continue_waiting_){
        return;
    }
    this->continue_waiting_ = true;
    this->waiting_future = std::async(std::launch::async,&file_change_notifier::waiting_function,this);

}

void file_change_notifier::waiting_function(){
    while (this->continue_waiting_){
        if (!FindNextChangeNotification(this->notification_handle_)){
            throw win32_exception("FindNextChangeNotification failed with \"",
                                  GetLastError());
        }
        DWORD wait_return = WaitForSingleObject(this->notification_handle_,1000);
        switch (wait_return) {
        case WAIT_OBJECT_0:
            this->callback_(this->directory_path_ + this->file_name_,this->events_);
            break;
        case WAIT_TIMEOUT:
            break;
        default:
            throw win32_exception("WaitForSingleObject returned unexpected: ", GetLastError());
            break;
        }
    }
}

file_change_notifier::win32_exception::win32_exception(std::string msg, std::uint32_t get_last_error_code) :
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



