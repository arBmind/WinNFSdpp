#pragma once

#include "binary/binary.h"
#include "binary/binary_reader.h"

#include "container/range_map.h"

#include <functional>
#include <string>

struct rpc_program_t {
  struct procedure_result_t {
    enum status_t { INVALID_ARGUMENTS, RESPONDED };

    static procedure_result_t respond(const binary_t& binary) {
      procedure_result_t result;
      result.status = RESPONDED;
      result.response = binary;
      return result;
    }

    status_t status = INVALID_ARGUMENTS;
    binary_t response;
  };

  struct procedure_args_t {
    std::string sender;
    binary_reader_t parameter_reader;
  };
  using procedure_callback_t = std::function<procedure_result_t (procedure_args_t&)>;

  struct procedure_t {
    const char* name /*= nullptr*/; // defaulting would inhibit VS from creating default constructor
    procedure_callback_t callback;
  };
  using procedure_map_t = range_map_t<procedure_t>;

public:
  uint32_t id;
  uint32_t version;
  procedure_map_t procedures;
};
