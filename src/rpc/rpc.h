#pragma once

#include "binary/binary_reader.h"
#include "binary/binary_builder.h"

#include "meta/variant.h"
#include "xdr.h"

#include <limits>
#include <cassert>

/**
 * @brief rpc implementation according to RFC1057
 *
 * https://www.ietf.org/rfc/rfc1057.txt
 * https://tools.ietf.org/html/rfc1057
 */
namespace rpc
{
  enum {
    VERSION = 2,
  };

  enum class msg_type_t { CALL = 0, REPLY = 1 };
  enum class reply_stat_t { ACCEPTED = 0, DENIED = 1 };
  enum class accept_stat_t {
    SUCCESS       = 0, /* RPC executed successfully       */
    PROG_UNAVAIL  = 1, /* remote hasn't exported program  */
    PROG_MISMATCH = 2, /* remote can't support version #  */
    PROC_UNAVAIL  = 3, /* program can't support procedure */
    GARBAGE_ARGS  = 4  /* procedure can't decode params   */
  };
  enum class reject_stat_t : uint32_t {
    RPC_MISMATCH = 0, /* RPC version number != 2          */
    AUTH_ERROR = 1    /* remote can't authenticate caller */
  };
  enum class auth_stat_t : uint32_t {
    BADCRED      = 1,  /* bad credentials (seal broken) */
    REJECTEDCRED = 2,  /* client must begin new session */
    BADVERF      = 3,  /* bad verifier (seal broken)    */
    REJECTEDVERF = 4,  /* verifier expired or replayed  */
    TOOWEAK      = 5   /* rejected for security reasons */
  };
  enum class auth_flavor_t : uint32_t {
    NONE       = 0,
    UNIX       = 1,
    SHORT      = 2,
    DES        = 3,
  };

  // ---- reader part ----

  struct opaque_auth_reader_t {
    opaque_auth_reader_t() = default;
    opaque_auth_reader_t(const binary_reader_t &reader)
      : reader_m(reader)
      , body_m(xdr::opaque_reader<400>(reader, 4))
    {}

    size_t size() const { return 4 + body_m.read_size(); }
    bool valid() const { return reader_m.has_size(size()); }

    auth_flavor_t flavor() const { return (auth_flavor_t)reader_m.get32(0); }
    binary_reader_t body() const { return body_m; }

  private:
    binary_reader_t reader_m;
    xdr::opaque_reader_t body_m;
  };

  struct call_body_t {
    call_body_t() = default;
    uint32_t rpc_version;
    uint32_t program;
    uint32_t version;
    uint32_t procedure;
    opaque_auth_reader_t credential_reader;
    opaque_auth_reader_t verification_reader;
    binary_reader_t parameter_reader;
  };

  struct call_body_reader_t {
    call_body_reader_t(const binary_reader_t& reader)
      : reader_m(reader)
      , credendial_m(reader.get_reader(16))
      , verification_m(reader.get_reader(16 + credendial_m.size()))
    {}

    size_t size() const { return 16 + credendial_m.size() + verification_m.size(); }
    bool valid() const { return reader_m.has_size(size()); }

    call_body_t read() const {
      call_body_t result;
      result.rpc_version = reader_m.get32(0);
      result.program = reader_m.get32(4);
      result.version = reader_m.get32(8);
      result.procedure = reader_m.get32(12);
      result.credential_reader = credendial_m;
      result.verification_reader = verification_m;
      result.parameter_reader = reader_m.get_reader(size());
      return result;
    }

  private:
    binary_reader_t reader_m;
    opaque_auth_reader_t credendial_m;
    opaque_auth_reader_t verification_m;
  };

  struct message_t {
    uint32_t xid;
    meta::variant_t<call_body_reader_t/*, reply_body_reader_t*/> body_reader;
  };

  struct message_reader_t {
    message_reader_t(const binary_reader_t& reader)
      : reader_m(reader)
    {}

    size_t size() const { return 8; }
    bool valid() const { return reader_m.has_size(size()); }

    message_t read() const {
      message_t result;
      result.xid = reader_m.get32(0);
      auto type = (msg_type_t)reader_m.get32(4);
      if (type == msg_type_t::CALL) {
          auto reader = call_body_reader_t(reader_m.get_reader(8));
          result.body_reader.set(reader);
        }
      return result;
    }

  private:
    binary_reader_t reader_m;
  };

  inline message_reader_t message_reader(const binary_reader_t& reader) {
    return { reader };
  }

  // ---- writer part ----

  struct mismatch_info_t {
    uint32_t low;
    uint32_t high;
  };

  struct auth_accepted_reply_builder_t {
    binary_t success(const binary_t& binary) {
      builder_m.append32(accept_stat_t::SUCCESS);
      builder_m.append_binary(binary);
      return builder_m.build();
    }

    binary_t program_unavailable() {
      builder_m.append32(accept_stat_t::PROG_UNAVAIL);
      return builder_m.build();
    }

    binary_t program_mismatch(const mismatch_info_t& mismatch) {
      builder_m.append32(accept_stat_t::PROG_MISMATCH);
      builder_m.append32(mismatch.low);
      builder_m.append32(mismatch.high);
      return builder_m.build();
    }

    binary_t procedure_unavailable() {
      builder_m.append32(accept_stat_t::PROC_UNAVAIL);
      return builder_m.build();
    }

    binary_t garbage_args() {
      builder_m.append32(accept_stat_t::GARBAGE_ARGS);
      return builder_m.build();
    }

    binary_builder_t builder_m;
  };

  inline void write_auth_null(binary_builder_t& builder) {
    builder.append32(auth_flavor_t::NONE);
    builder.append32(0);
  }

  struct accepted_reply_builder_t {
    auth_accepted_reply_builder_t null_auth() {
      write_auth_null(builder_m);
      return { builder_m };
    }

    binary_builder_t builder_m;
  };

  struct rejected_reply_builder_t {
    binary_t mismatch(const mismatch_info_t& mismatch) {
      builder_m.append32(reject_stat_t::RPC_MISMATCH);
      builder_m.append32(mismatch.low);
      builder_m.append32(mismatch.high);
      return builder_m.build();
    }

    binary_t auth_error(auth_stat_t auth_stat) {
      builder_m.append32(reject_stat_t::AUTH_ERROR);
      builder_m.append32(auth_stat);
      return builder_m.build();
    }

    binary_builder_t builder_m;
  };

  struct reply_body_builder_t {
    accepted_reply_builder_t accept() {
      builder_m.append32(reply_stat_t::ACCEPTED);
      return { builder_m };
    }

    rejected_reply_builder_t reject() {
      builder_m.append32(reply_stat_t::DENIED);
      return { builder_m };
    }

    binary_builder_t builder_m;
  };

  struct message_builder_t {
    // TODO
    //		call_body_builder_t call(uint32_t xid) {
    //			builder_m.append32(xid);
    //			builder_m.append32(msg_type_t::REPLY);
    //			return { builder_m };
    //		}

    reply_body_builder_t reply(uint32_t xid) {
      builder_m.append32(xid);
      builder_m.append32(msg_type_t::REPLY);
      return { builder_m };
    }

  private:
    binary_builder_t builder_m;
  };

  inline message_builder_t message_builder() {
    return {};
  }
} // namespace rpc
