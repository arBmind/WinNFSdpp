#include "portmap.h"

#include "rpc/rpc_router.h"

#include "binary/binary_builder.h"

#include <algorithm>

#define GOOGLE_GLOG_DLL_DECL
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>

namespace portmap {
  namespace {
    template<typename store_t>
    auto find(store_t &store, const mapping_t& mapping) {
      return std::find_if(store.begin(), store.end(), [&](const mapping_t& candidate) {
          return candidate.program == mapping.program
              && candidate.version == mapping.version
              && candidate.protocol == mapping.protocol;
        });
    }

    enum {
      MAPPING_SIZE = 16,
    };

    bool read_mapping(const binary_reader_t& reader, mapping_t& mapping) {
      if (!reader.has_size(MAPPING_SIZE)) return false;
      mapping.program = reader.get32(0);
      mapping.version = reader.get32(4);
      mapping.protocol = reader.get32(8);
      mapping.port = reader.get32(12);
      return true;
    }

  } // namespace

  bool rpc_program::set(const mapping_t& mapping)
  {
    auto it = find(store_m, mapping);
    if (it != store_m.end()) return false;
    store_m.push_back(mapping);
    return true;
  }

  bool rpc_program::unset(const mapping_t& mapping)
  {
    auto it = find(store_m, mapping);
    if (it == store_m.end()) return false;
    store_m.erase(it);
    return true;
  }

  uint32_t rpc_program::get_port(const mapping_t& mapping) const
  {
    LOG(INFO) << "GetPort: program: " << mapping.program << " version: " << mapping.version << " prot: " << mapping.protocol;
    auto it = find(store_m, mapping);
    if (it == store_m.end()) return 0;
    LOG(INFO) << "... Success port: " << it->port << std::endl;
    return it->port;
  }

  rpc_program_t rpc_program::describe()
  {
    using args_t = rpc_program_t::procedure_args_t;
    using result_t = rpc_program_t::procedure_result_t;

    rpc_program_t result;
    result.id = PROGRAM;
    result.version = VERSION;

    auto null_rpc = [=](const args_t& args)->result_t {
        if (0 != args.parameter_reader.size()) return {};
        nothing();
        return result_t::respond({});
      };
    auto set_rpc = [=](const args_t& args)->result_t {
        mapping_t mapping;
        if (!read_mapping(args.parameter_reader, mapping)) return {};
        if (mapping.port != protocol_t::TCP && mapping.port != protocol_t::UDP) return {};
        auto result = set(mapping);
        binary_builder_t builder;
        builder.append32(result);
        return result_t::respond(builder.build());
      };
    auto unset_rpc = [=](const args_t& args)->result_t {
        mapping_t mapping;
        if (!read_mapping(args.parameter_reader, mapping)) return {};
        if (mapping.port != protocol_t::TCP && mapping.port != protocol_t::UDP) return {};
        auto result = unset(mapping);
        binary_builder_t builder;
        builder.append32(result);
        return result_t::respond(builder.build());
      };
    auto get_port_rpc = [=](const args_t& args)->result_t {
        mapping_t mapping;
        if (!read_mapping(args.parameter_reader, mapping)) return {};
        auto result = get_port(mapping);
        binary_builder_t builder;
        builder.append32(result);
        return result_t::respond(builder.build());
      };

    auto& calls = result.procedures;
    calls.set(0, { "NULL", null_rpc });
    calls.set(1, { "SET", set_rpc });
    calls.set(2, { "UNSET", unset_rpc });
    calls.set(3, { "GETPORT", get_port_rpc });
    //calls.set(4, { "DUMP", dump_rpc });
    //calls.set(5, { "CALLIT", callit_rpc });

    return result;
  }

} // namespace portmap
