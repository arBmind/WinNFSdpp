#pragma once

#include "binary/binary.h"
#include "rpc/rpc_program.h"

#include "mount_cache.h"
#include "wintime/wintime_convert.h"

#include "meta/variant.h"

#include <string>
#include <cstdint>
#include "winfs/windows_error.h"

/**
 * @brief nfs implementation according to RFC1813
 *
 * https://www.ietf.org/rfc/rfc1813.txt
 * https://tools.ietf.org/html/rfc1813
 */
namespace nfs3
{
  enum {
    PROGRAM = 100003,
    VERSION = 3,
    PORT = 2049,

    FILEHANDLE_SIZE = 64, // The maximum size in bytes of the opaque file handle.
    COOKIEVERF_SIZE = 8, // The size in bytes of the opaque cookie verifier passed by READDIR and READDIRPLUS.
    CREATEVERF_SIZE = 8, // The size in bytes of the opaque verifier used for exclusive CREATE.
    WRITEVERF_SIZE = 8, // The size in bytes of the opaque verifier used for asynchronous WRITE.
  };

  using filename_t = std::string;
  using path_t = std::string;

  using fileid_t = uint64_t;
  using cookie_t = uint64_t;
  using cookie_verifier_t = std::array<uint8_t, COOKIEVERF_SIZE>;
  using create_verifier_t = std::array<uint8_t, CREATEVERF_SIZE>;
  using write_verifier_t = std::array<uint8_t, WRITEVERF_SIZE>;
  using uid_t = uint32_t;
  using gid_t = uint32_t;
  using size_t = uint64_t;
  using offset_t = uint64_t;
  using mode_t = uint32_t;
  using count_t = uint32_t;

  enum class status_t : uint32_t {
    OK              = 0, // Indicates the call completed successfully.
    ERR_PERM        = 1, // Not owner. The operation was not allowed.
    ERR_NO_ENTRY    = 2, // No such file or directory.
    ERR_IO          = 5, // I/O error. A hard error (for example, a disk error)
    ERR_NXIO        = 6, // I/O error. No such device or address.
    ERR_ACCESS      = 13, // Permission denied. The caller does not have the correct permission
    ERR_EXIST       = 17, // File exists. The file specified already exists.
    ERR_XDEV        = 18, // Attempt to do a cross-device hard link.
    ERR_NODEV       = 19, // No such device.
    ERR_NOTDIR      = 20, // Not a directory.
    ERR_ISDIR       = 21, // Is a directory.
    ERR_INVAL       = 22, // Invalid argument or unsupported argument for an operation.
    ERR_FBIG        = 27, // File too large.
    ERR_NOSPC       = 28, // No space left on device.
    ERR_ROFS        = 30, // Read-only file system.
    ERR_MLINK       = 31, // Too many hard links.
    ERR_NAMETOOLONG = 63, // The filename in an operation was too long.
    ERR_NOTEMPTY    = 66, // An attempt was made to remove a directory that was not empty.
    ERR_DQUOT       = 69, // Resource (quota) hard limit exceeded.
    ERR_STALE       = 70, // Invalid file handle. The file handle given in the arguments was invalid.
    ERR_REMOTE      = 71, // Too many levels of remote in path.
    ERR_BADHANDLE   = 10001, // Illegal NFS file handle.
    ERR_NOT_SYNC    = 10002, // Update synchronization mismatch was detected during a SETATTR operation.
    ERR_BAD_COOKIE  = 10003, // READDIR or READDIRPLUS cookie is stale.
    ERR_NOTSUPP     = 10004, // Operation is not supported.
    ERR_TOOSMALL    = 10005, // Buffer or request is too small.
    ERR_SERVERFAULT = 10006, // An error occurred on the server which does not map to any of the legal NFS version 3 protocol error values.
    ERR_BADTYPE     = 10007, // An attempt was made to create an object of a type not supported by the server.
    ERR_JUKEBOX     = 10008, // The server initiated the request, but was not able to complete it in a timely fashion.
  };

  enum class filetype_t : uint32_t {
    REGULAR_FILE = 1, // regular file
    DIRECTORY    = 2, // directory
    BLK    = 3, // block special device file
    CHR    = 4, // character special device file
    SYMLINK      = 5, // symbolic link
    SOCK   = 6, // socket
    FIFO   = 7, // named pipe
  };

  enum mode_flags {
    SET_USER_ID       = 0x00800,
    SET_GROUP_ID      = 0x00400,
    SAVE_SWAPPED_TEXT = 0x00200,
    OWNER_READ        = 0x00100,
    OWNER_WRITE       = 0x00080,
    OWNER_EXECUTE     = 0x00040,
    GROUP_READ        = 0x00020,
    GROUP_WRITE       = 0x00010,
    GROUP_EXECUTE     = 0x00008,
    OTHERS_READ       = 0x00004,
    OTHERS_WRITE      = 0x00002,
    OTHERS_EXECUTE    = 0x00001,
  };

  using filehandle_t = binary_t; // maximum FHSIZE

  struct specdata_t {
    uint32_t data1 = 0;
    uint32_t data2 = 0;
  };

  using time_t = wintime::unix_time_t;
  struct file_attr_t {
    filetype_t type;
    mode_t mode;
    uint32_t nlink;
    uid_t uid;
    gid_t gid;
    size_t size;
    size_t used;
    specdata_t rdev;
    uint64_t fsid;
    fileid_t fileid;
    time_t atime; // access time
    time_t mtime; // modified time
    time_t ctime; // create time
  };
  struct wcc_attr_t { // weak cache consistency
    size_t size;
    time_t mtime;
    time_t ctime;
  };
  using post_op_attr_t = meta::optional_t<file_attr_t>;
  using pre_op_attr_t = meta::optional_t<wcc_attr_t>;

  struct wcc_data_t { // weak cache consistency
    pre_op_attr_t before;
    post_op_attr_t after;
  };

  using post_op_filehandle_t = meta::optional_t<filehandle_t>;

  enum class time_how_t : uint32_t {
    DONT_CHANGE        = 0,
    SET_TO_SERVER_TIME = 1,
    SET_TO_CLIENT_TIME = 2
  };

  struct set_attr_t {
    meta::optional_t<mode_t> mode;
    meta::optional_t<uid_t> uid;
    meta::optional_t<gid_t> gid;
    meta::optional_t<size_t> size;
    time_how_t set_atime;
    time_t atime;
    time_how_t set_mtime;
    time_t mtime;
  };

  struct dir_op_args_t {
    filehandle_t directory;
    filename_t name;
  };

  using get_attr_args_t = dir_op_args_t;
  struct get_attr_result_t {
    status_t status = status_t::ERR_ACCESS;
    file_attr_t attr; // only valid for stat OK
  };

  struct set_attr_args_t {
    filehandle_t filehandle;
    set_attr_t new_attributes;
    meta::optional_t<time_t> check_time;
  };
  struct set_attr_result_t {
    status_t status = status_t::ERR_ACCESS;
    wcc_data_t wcc_data;
  };

  using lookup_args_t = dir_op_args_t;
  struct lookup_result_t {
    status_t status = status_t::ERR_ACCESS;
    filehandle_t object_handle; // only valid for stat OK
    post_op_attr_t object_attributes; // only valid for stat OK
    post_op_attr_t dir_attributes;
  };

  enum access_flags {
    ACCESS_READ    = 0x0001, // Read data from file or read a directory.
    ACCESS_LOOKUP  = 0x0002, // Look up a name in a directory (no meaning for non-directory objects).
    ACCESS_MODIFY  = 0x0004, // Rewrite existing file data or modify existing directory entries.
    ACCESS_EXTEND  = 0x0008, // Write new data or add directory entries.
    ACCESS_DELETE  = 0x0010, // Delete an existing directory entry.
    ACCESS_EXECUTE = 0x0020, // Execute file (no meaning for a directory).
  };

  struct access_args_t {
    filehandle_t filehandle;
    uint32_t access;
  };
  struct access_result_t {
    status_t status = status_t::ERR_ACCESS;
    post_op_attr_t obj_attributes;
    uint32_t access = 0; // only valid for stat OK
  };

  using readlink_args_t = dir_op_args_t;
  struct readlink_result_t {
    status_t status = status_t::ERR_ACCESS;
    post_op_attr_t symlink_attributes;
    path_t path; // only valid for stat OK
  };

  struct read_args_t {
    filehandle_t filehandle;
    offset_t offset;
    count_t count;
  };
  struct read_result_t {
    status_t status = status_t::ERR_ACCESS;
    post_op_attr_t file_attributes;
    count_t count;
    bool eof;
    binary_t data;
  };

  enum class stable_how_t : uint32_t {
    UNSTABLE = 0,
    DATA_SYNC = 1,
    FILE_SYNC = 2,
  };
  struct write_args_t {
    filehandle_t filehandle;
    offset_t offset;
    count_t count;
    stable_how_t stable;
    binary_t data;
  };
  struct write_result_t {
    status_t status = status_t::ERR_ACCESS;
    wcc_data_t file_wcc;
    count_t count;
    stable_how_t committed;
    write_verifier_t verifier;
  };

  enum class create_how_t : uint32_t {
    UNCHECKED = 0,
    GUARDED = 1,
    EXCLUSIVE = 2,
  };
  struct create_args_t {
    dir_op_args_t where;
    create_how_t how;
    meta::optional_t<set_attr_t> obj_attributes; // UNCHECKED || GUARDED
    meta::optional_t<create_verifier_t> verifier; // EXCLUSIVE
  };
  struct create_result_t {
    status_t status = status_t::ERR_ACCESS;
    post_op_filehandle_t object;
    post_op_attr_t object_attributes;
    wcc_data_t directory_wcc;
  };

  struct mkdir_args_t {
    dir_op_args_t where;
    set_attr_t attributes;
  };
  using mkdir_result_t = create_result_t;

  using remove_args_t = dir_op_args_t;
  struct remove_result_t {
    status_t status = status_t::ERR_ACCESS;
    wcc_data_t directory_wcc;
  };

  using rmdir_args_t = dir_op_args_t;
  using rmdir_result_t = remove_result_t;

  struct rename_args_t {
    dir_op_args_t from;
    dir_op_args_t to;
  };
  struct rename_result_t {
    status_t status = status_t::ERR_ACCESS;
    wcc_data_t from_directory_wcc;
    wcc_data_t to_directory_wcc;
  };

  struct read_dir_args_t {
    filehandle_t directory;
    cookie_t cookie;
    cookie_verifier_t cookie_verifier;
    uint32_t count;
  };
  struct read_dir_entry_t {
    fileid_t file_id;
    filename_t name;
    cookie_t cookie;
  };
  struct read_dir_result_t {
    status_t status = status_t::ERR_ACCESS;
    post_op_attr_t directory_attributes;
    cookie_verifier_t cookie_verifier; // only valid for status OK
    std::vector<read_dir_entry_t> reply;
    bool is_finished = true;
  };

  struct read_dir_plus_args_t {
    filehandle_t directory;
    cookie_t cookie;
    cookie_verifier_t cookie_verifier;
    uint32_t dircount;
    uint32_t maxcount;
  };
  struct read_dir_plus_entry_t {
    fileid_t file_id;
    filename_t name;
    cookie_t cookie;
    post_op_attr_t name_attributes;
    post_op_filehandle_t name_handle;
  };
  struct read_dir_plus_result_t {
    status_t status = status_t::ERR_ACCESS;
    post_op_attr_t directory_attributes;
    cookie_verifier_t cookie_verifier; // only valid for status OK
    std::vector<read_dir_plus_entry_t> reply;
    bool is_finished = true;
  };

  struct fs_stat_result_t {
    status_t status = status_t::ERR_ACCESS;
    post_op_attr_t object_attributes;
    size_t total_bytes; // total size, in bytes, of the file system
    size_t free_bytes; // amount of free space, in bytes, in the file system
    size_t available_bytes; // free space, in bytes, available to the user
    size_t total_files; // total number of file slots in the file system
    size_t free_files; // number of free file slots in the file system
    size_t available_files; // free file slots that are available to the user
    uint32_t invar_sec = 0; // number of seconds for which the file system is not expected to change
  };

  enum fs_info_property_flags {
    FS_INFO_LINK,
    FS_INFO_SYMLINK,
    FS_INFO_HOMOGENOUS,
    FS_INFO_CANSETTIME,
  };
  struct fs_info_result_t {
    status_t status = status_t::ERR_ACCESS;
    post_op_attr_t object_attributes;
    uint32_t read_max_size = 64 * 512;
    uint32_t read_preferred_size = 64 * 512;
    uint32_t read_suggested_multiple = 512;
    uint32_t write_max_size = 8 * 512;
    uint32_t write_preferred_size = 8 * 512;
    uint32_t write_suggested_multiple = 512;
    uint32_t directory_preferred_size = 8 * 1024;
    size_t file_maximum_size = 1024ull * 1024 * 1024 * 1024; // 1 TB
    time_t time_delta = { 0, 100 }; // 100 nanoseconds
    uint32_t properties = FS_INFO_HOMOGENOUS;
  };

  struct path_conf_result_t {
    status_t status = status_t::ERR_ACCESS;
    post_op_attr_t object_attributes;
    uint32_t link_max = 1; // maximum number of hard links to an object
    uint32_t name_max = 255; // maximum length of a component of a filename.
    bool no_truncation = true; // reject any request that includes a name longer than name_max
    bool chown_restricted = true; // server will reject any request to change either the owner or the group
    bool case_insensitive = true; // server file system does not distinguish case when interpreting filenames
    bool case_preserving = true; // server file system will preserve the case of a name
  };

  struct commit_args_t {
    filehandle_t file;
    offset_t offset;
    count_t count;
  };

  struct commit_result_t {
    status_t status = status_t::ERR_ACCESS;
    wcc_data_t file_wcc;
    write_verifier_t verifier;
  };

  struct rpc_program
  {
    rpc_program(const mount_cache_t& mount_cache);

  public: // RPC implementations
    void nothing() const {} // null
    get_attr_result_t get_attr(const filehandle_t&);
    set_attr_result_t set_attr(const set_attr_args_t&);

    lookup_result_t lookup(const dir_op_args_t&);
    access_result_t access(const access_args_t&);
    readlink_result_t readlink(const filehandle_t&);
    read_result_t read(const read_args_t&);
    write_result_t write(const write_args_t&);
    create_result_t create(const create_args_t&);
    mkdir_result_t mkdir(const mkdir_args_t&);
    //symlink_result_t symlink(const symlink_args_t&);
    //mknod_result_t mknod(const mknod_args_t&);
    remove_result_t remove(const dir_op_args_t&);
    rmdir_result_t rmdir(const dir_op_args_t&);
    rename_result_t rename(const rename_args_t&);
    //link_result_t link(const link_args_t&);
    read_dir_result_t read_dir(const read_dir_args_t&);
    read_dir_plus_result_t read_dir_plus(const read_dir_plus_args_t&);
    fs_stat_result_t fs_stat(const filehandle_t& root);
    fs_info_result_t fs_info(const filehandle_t& root);
    path_conf_result_t path_conf(const filehandle_t&);
    commit_result_t commit(const commit_args_t&);

  public: // management
    rpc_program_t describe();

  private:
    const mount_cache_t& mount_cache_m;
    cookie_verifier_t cookie_verifier_m;
  };

  status_t to_nfs3_error(windows::win32_error_code_t win32_error);

} // namespace nfs3
