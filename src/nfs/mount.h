#pragma once

#include "mount_aliases.h"
#include "mount_cache.h"

#include "binary/binary.h"
#include "rpc/rpc_program.h"

#include <string>
#include <cstdint>

namespace mount {

  enum {
    PROGRAM = 100005,
    VERSION = 3,
    PORT = 1058,

    DIRECTORY_PATH_LEN = 1024, // Maximum bytes in a path name
    MNTNAMLEN  = 255,  // Maximum bytes in a name
    FILEHANDLE_SIZE    = 64,   // Maximum bytes in a V3 file handle
  };

  using filehandle_t = binary_t;
  using hostname_t = std::string;
  using directory_path_t = std::string;

  enum status_t {
    OK = 0,                  /* no error */
    ERR_PERM = 1,            /* Not owner */
    ERR_NOENT = 2,           /* No such file or directory */
    ERR_IO = 5,              /* I/O error */
    ERR_ACCES = 13,          /* Permission denied */
    ERR_NOTDIR = 20,         /* Not a directory */
    ERR_INVAL = 22,          /* Invalid argument */
    ERR_NAMETOOLONG = 63,    /* Filename too long */
    ERR_NOTSUPP = 10004,     /* Operation not supported */
    ERR_SERVERFAULT = 10006  /* A failure on the server */
  };

  struct mount_result_t {
    status_t status = ERR_NOENT;
    filehandle_t filehandle; // only valid for OK
    binary_t auth_flavors; // only valid for OK
  };

  struct mount_entry_t {
    hostname_t hostname;
    directory_path_t directory;
  };

  struct rpc_program {

  public: // RPC implementations
    void nothing() const {} // null
    mount_result_t mount(const hostname_t& sender, const directory_path_t&);
    //mount_list_t dump();
    void unmount(const hostname_t& sender, const directory_path_t&);
    void unmount_all(const hostname_t& sender);
    //exports_t export();

  public: // management
    const mount_cache_t& cache() const { return mount_cache_m; }
    mount_aliases_t& aliases() { return mount_aliases_m; }
    void restore(const binary_t& binary) { mount_cache_m.restore(binary); }

    rpc_program_t describe();

  private:
    mount_aliases_t mount_aliases_m;
    mount_cache_t mount_cache_m;
  };

} // namespace mount
