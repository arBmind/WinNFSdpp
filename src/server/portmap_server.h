#pragma once

#include "rpc_server.h"

#include "rpc/portmap.h"

struct portmap_server_t {
  portmap_server_t()
    : rpc_server_m(portmap::PORT)
  {}

  void add(int program, int version, int port) {
    portmap::mapping_t m;
    m.program = program;
    m.version = version;
    m.port = port;
    m.protocol = portmap::protocol_t::TCP;
    program_m.set(m);
    m.protocol = portmap::protocol_t::UDP;
    program_m.set(m);
  }

  void start() {
    rpc_server_m.add(program_m.describe());
    rpc_server_m.start();
  }

private:
  portmap::rpc_program program_m;
  rpc_server_t rpc_server_m;
};
