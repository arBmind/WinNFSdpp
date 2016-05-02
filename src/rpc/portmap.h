#pragma once

#include "rpc_program.h"

#include <cstdint>
#include <vector>

namespace portmap {

  enum {
    PROGRAM = 100000,
    VERSION = 2,
    PORT = 111, // SUNRPC
  };
  enum protocol_t {
    TCP = 6,
    UDP = 17,
  };

  struct mapping_t
  {
    uint32_t program;
    uint32_t version;
    uint32_t protocol;
    uint32_t port;
  };

  struct rpc_program
  {
    using store_t = std::vector<mapping_t>;

  public: // RPC implementations
    void nothing() const {} // null
    bool set(const mapping_t&);
    bool unset(const mapping_t&);
    uint32_t get_port(const mapping_t&) const;
    store_t dump() const { return store_m; }
    //    result_t callit(const call_args_t&) const;

  public: // management
    rpc_program_t describe();

  private:
     store_t store_m;
  };

} // namespace portmap
