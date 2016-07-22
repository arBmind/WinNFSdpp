#include "rpc_router.h"

#include "rpc.h"

#include <glog/logging.h>

binary_t rpc_router_t::handle(const router_args_t& server_args) const
{
  using procedure_args_t = rpc_program_t::procedure_args_t;
  using procedure_result_t = rpc_program_t::procedure_result_t;

  auto message_reader = rpc::message_reader(server_args.request_reader);
  if ( !message_reader.valid()) {
      LOG(WARNING) << "RPC_ROUTER invalid request" << std::endl;
      return {}; // no message - no reply
    }
  auto message = message_reader.read();
  if ( !message.body_reader.is<rpc::call_body_reader_t>()) {
      LOG(WARNING) << "RPC_ROUTER invalid message" << std::endl;
      return {}; // no call - no reply
    }

  auto call_body_reader = message.body_reader.get<rpc::call_body_reader_t>();
  if ( !call_body_reader.valid()) {
      LOG(WARNING) << "RPC_ROUTER invalid call body" << std::endl;
      return {}; // failed to read body - no reply
    }
  auto reply = rpc::message_builder().reply(message.xid);
  auto call_body = call_body_reader.read();
  if (call_body.rpc_version != rpc::VERSION) {
      LOG(WARNING) << "RPC_ROUTER invalid RPC version" << std::endl;
      return reply.reject().mismatch({ rpc::VERSION, rpc::VERSION });
    }
  auto accept_reply = reply.accept();
  // TODO: perform auth check
  auto auth_reply = accept_reply.null_auth();

  // find the procedure to call
  auto program_it = program_map_m.find(call_body.program);
  if (program_it == program_map_m.end()) {
      LOG(WARNING) << "RPC_ROUTER unkown program: " << call_body.program << " v" << call_body.version << std::endl;
      return auth_reply.program_unavailable();
    }
  auto& version_map = program_it->second;
  if (! version_map.contains(call_body.version)) {
      LOG(WARNING) << "RPC_ROUTER unkown program version: " << call_body.program << " v" << call_body.version << std::endl;
      return auth_reply.program_mismatch({version_map.range_start(), version_map.range_end() - 1});
    }
  auto& procedure_map = version_map[call_body.version];
  if (! procedure_map.contains(call_body.procedure)) {
      LOG(WARNING) << "RPC_ROUTER unkown procedure in program: " << call_body.program << " v" << call_body.version << " procedure: " << call_body.procedure << std::endl;
      return auth_reply.procedure_unavailable();
    }

  auto& procedure = procedure_map[call_body.procedure];
  if (! procedure.callback || ! procedure.name) {
      LOG(WARNING) << "RPC_ROUTER unkown procedure in program: " << call_body.program << " v" << call_body.version << " procedure: ";
      if (procedure.name) LOG(WARNING) << procedure.name; else LOG(WARNING) << call_body.procedure;
      LOG(WARNING) << std::endl;
      return auth_reply.procedure_unavailable();
    }
  procedure_args_t procedure_args { server_args.sender, call_body.parameter_reader };
  auto procedure_result = procedure.callback(procedure_args);

  DLOG(INFO) << "Calling procedure " << procedure.name << " by sender " << procedure_args.sender
             << " and results "
             << (binary_reader_t::binary(procedure_result.response).valid() ?
                     binary_reader_t::binary(procedure_result.response).get_hex_string(0,4)
                        : " invalid results");
  if (procedure_result.status == procedure_result_t::INVALID_ARGUMENTS) {
      std::stringstream error_log_message;
      error_log_message << "RPC_ROUTER garbage args: " << call_body.program << " v" << call_body.version << " procedure: ";
      if (procedure.name) LOG(WARNING) << procedure.name; else LOG(WARNING) << call_body.procedure;
      LOG(WARNING) << error_log_message.str();
      return auth_reply.garbage_args();
    }
  return auth_reply.success(procedure_result.response);
}

void rpc_router_t::add(const rpc_program_t &program)
{
  program_map_m[program.id].set(program.version, program.procedures);
}
