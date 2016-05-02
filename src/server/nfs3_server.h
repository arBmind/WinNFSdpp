#pragma once

#include "rpc_server.h"

#include "nfs/nfs3.h"

struct nfs3_server_t {
  nfs3_server_t(const mount_cache_t& mount_cache)
    : program_m(mount_cache)
    , rpc_server_m(nfs3::PORT)
  {}

  void start() {
    rpc_server_m.add(program_m.describe());
    rpc_server_m.start();
  }

private:
  nfs3::rpc_program program_m;
  rpc_server_t rpc_server_m;
};
