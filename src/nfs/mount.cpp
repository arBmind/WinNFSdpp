#include "mount.h"

#include "rpc/rpc.h"

#define DEBUG_NFS_MOUNT_RPC

#ifdef DEBUG_NFS_MOUNT_RPC
#include <iostream>
#endif

namespace mount {
  namespace {
    struct directory_path_reader_t {
      directory_path_reader_t(const binary_reader_t& reader)
        : directory_path_m(xdr::opaque_reader<DIRECTORY_PATH_LEN>(reader, 0))
      {}

      size_t size() const { return 4 + directory_path_m.read_size(); }
      bool valid() const { return directory_path_m.valid(); }
      directory_path_t directory_path() const { return directory_path_m.to_string(); }

    private:
      xdr::opaque_reader_t directory_path_m;
    };

    binary_t write_mount_result(const mount_result_t& mount_result) {
      binary_builder_t builder;
      builder.append32(mount_result.status);
      if (mount_result.status == status_t::OK) {
          xdr::write_opaque_binary<FILEHANDLE_SIZE>(builder, mount_result.filehandle);
          xdr::write_opaque_binary<>(builder, mount_result.auth_flavors);
        }
      return builder.build();
    }

  } // namespace

  mount_result_t rpc_program::mount(const hostname_t& sender, const directory_path_t& directory_path) {
    std::cout << "Mount: " << directory_path << " for " << sender << std::endl;
    mount_result_t result;

    mount_cache_m.mount_session([&](const auto& session) {
        auto mount_it = session.find_query(directory_path);
        if (mount_it != session.end()) {
            session.mount_sender(sender, mount_it);
          }
        else {
            auto windows_path = mount_aliases_m.resolve(directory_path);
            mount_it = session.find_windows(windows_path);

            if (mount_it != session.end()) {
                session.mount_sender_query(sender, mount_it, directory_path);
              }
            else {
                mount_it = session.mount(sender, directory_path, windows_path);
              }
          }

        if (mount_it != session.end()) {
            std::cout << "Mount success!" << std::endl;
            result.status = status_t::OK;
            result.filehandle = mount_it->second.filehandle;
            // result.auth_flavors = ??
          }
      });

    return result;
  }

  void rpc_program::unmount(const hostname_t& sender, const directory_path_t& directory_path)
  {
    std::cout << "Unmount: " << directory_path << " for " << sender << std::endl;
    mount_cache_m.unmount(sender, directory_path);
  }

  void rpc_program::unmount_all(const hostname_t& sender)
  {
    std::cout << "Unmount All: " << sender << std::endl;
    mount_cache_m.unmount_client(sender);
  }

  rpc_program_t rpc_program::describe() {
    using args_t = rpc_program_t::procedure_args_t;
    using result_t = rpc_program_t::procedure_result_t;

    rpc_program_t result;
    result.id = PROGRAM;
    result.version = VERSION;

    auto null_rpc = [=](const args_t& args)->result_t {
        if (!args.parameter_reader.empty()) return {};
        nothing();
        return result_t::respond({});
      };
    auto mount_rpc = [=](const args_t& args)->result_t {
        directory_path_reader_t reader(args.parameter_reader);
        if (!reader.valid()) return {};
        auto mount_result = mount(args.sender, reader.directory_path());
        return result_t::respond(write_mount_result(mount_result));
      };
    //    auto dump_rpc = [=](const args_t& args)->result_t {
    //        if (0 != args.parameter_reader.size()) return {};
    //        auto dump_result = dump();
    //        return result_t::respond(write_dump_result(dump_result));
    //      };
    auto unmount_rpc = [=](const args_t& args)->result_t {
        directory_path_reader_t reader(args.parameter_reader);
        if (!reader.valid()) return {};
        unmount(args.sender, reader.directory_path());
        return result_t::respond({});
      };
    auto unmountall_rpc = [=](const args_t& args)->result_t {
        if (0 != args.parameter_reader.size()) return {};
        unmount_all(args.sender);
        return result_t::respond({});
      };
    //    auto export_rpc = [=](const args_t& args)->result_t {
    //        if (0 != args.parameter_reader.size()) return {};
    //        auto export_result = export();
    //        return result_t::respond(write_export_result(export_result));
    //      };

    auto& calls = result.procedures;
    calls.set(0, { "NULL", null_rpc });
    calls.set(1, { "MNT", mount_rpc });
    //calls.set(2, { "DUMP", dump_rpc });
    calls.set(3, { "UMNT", unmount_rpc });
    calls.set(4, { "UMNTALL", unmountall_rpc });
    //calls.set(5, { "EXPORT", export_rpc });

    return result;
  }

} // namespace mount
