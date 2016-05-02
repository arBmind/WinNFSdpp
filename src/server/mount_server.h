#pragma once

#include "rpc_server.h"

#include "nfs/mount.h"

struct mount_server_t {
  mount_server_t()
    : rpc_server_m(mount::PORT)
  {}

  const mount_cache_t& cache() const { return program_m.cache(); }
  mount_aliases_t& aliases() { return program_m.aliases(); }
  void restore(const binary_t& binary) { program_m.restore(binary); }

  void start() {
    rpc_server_m.add(program_m.describe());
    rpc_server_m.start();
  }

private:
  mount::rpc_program program_m;
  rpc_server_t rpc_server_m;
};
