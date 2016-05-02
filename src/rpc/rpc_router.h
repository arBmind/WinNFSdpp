#pragma once

#include "rpc_program.h"

#include "container/range_map.h"

#include <map>

struct router_args_t
{
  std::string sender;
  binary_reader_t request_reader;
};

struct rpc_router_t
{
  binary_t handle(const router_args_t&) const;

  void add(const rpc_program_t&);

private:
  using procedure_map_t = rpc_program_t::procedure_map_t;
  using version_map_t = range_map_t<procedure_map_t>;
  using program_map_t = std::map<uint32_t, version_map_t>;

  program_map_t program_map_m;
};
