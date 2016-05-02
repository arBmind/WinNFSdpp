#pragma once

#include "rpc/rpc_program.h"

#include <memory>

struct rpc_server_t {
  rpc_server_t(int port);
  ~rpc_server_t();

  void add(const rpc_program_t&);

  void start();

private:
  struct impl;
  std::unique_ptr<impl> p;
};
