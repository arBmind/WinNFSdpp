#include "network/wsa_session.h"

#include "nfs/nfs3.h"
#include "nfs/mount.h"

#include "server/portmap_server.h"
#include "server/mount_server.h"
#include "server/nfs3_server.h"

#include <iostream>
#include <iterator>

#include "GSL/string_span.h"
#include <cwctype>
#include <fstream>
#include <cstdint>

#include <gflags/gflags.h>
#define GOOGLE_GLOG_DLL_DECL
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>

struct config_reader_t {
  config_reader_t(mount_aliases_t& aliases)
    : aliases_m(aliases)
  {}

  static void trim_space(gsl::cwstring_span<>& span) {
    while (!span.empty() && std::iswspace(span[0])) span = span.subspan(1);
    while (!span.empty() && std::iswspace(span.last<1>()[0])) span = span.subspan(0, span.size() - 1);
  }

  int read(std::string& filepath) {
    auto source = aliases_m.create_source();
    read(filepath, source);
    return source;
  }

  void read(const std::string& filepath, int source) {
    std::wifstream ifs(filepath);
    std::wstring line;
    mount_aliases_t::alias_vector_t aliases;
    while (std::getline(ifs, line)) {
        gsl::cwstring_span<> line_span(line);
        trim_space(line_span);
        if (line_span.empty() || line_span[0] == '#') continue;

        aliases.push_back(std::make_pair(gsl::to_string(line_span), std::string()));
      }
    aliases_m.set(source, aliases);
  }

private:
  mount_aliases_t& aliases_m;
};

struct program_t {
  program_t()
    : nfs3_server_m(mount_server_m.cache())
  {}

  void restore_cache() {
    std::ifstream ifs("Z:/mount_cache", std::ios_base::binary);
    binary_t binary;
    binary.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    if (!binary.empty()) {
        mount_server_m.restore(binary);
      }
  }

  void store_cache() {
    binary_t binary = mount_server_m.cache().save();
    std::ofstream ofs("Z:/mount_cache", std::ios_base::binary);
    ofs.write((char*)&binary[0], binary.size());
    ofs.flush();
  }

  void run() {
    portmap_server_m.add(mount::PROGRAM, mount::VERSION, mount::PORT);
    portmap_server_m.add(nfs3::PROGRAM, nfs3::VERSION, nfs3::PORT);

    auto source = mount_server_m.aliases().create_source();
    //mount_server_m.aliases().add(source, L"D:/arB/Ansible/hicknhack-private/mailserver-example");
    config_reader_t config_reader(mount_server_m.aliases());
    config_reader.read("C:/C/Bin/Vagrant/embedded/gems/gems/vagrant-1.8.1/nfspaths", source);

    restore_cache();

    portmap_server_m.start();
    mount_server_m.start();
    nfs3_server_m.start();
    cli_loop();

    store_cache();
  }

  void cli_loop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit" || line == "q") return;
      }
  }

private:
  portmap_server_t portmap_server_m;
  mount_server_t mount_server_m;
  nfs3_server_t nfs3_server_m;
};

#include "winfs/winfs_directory.h"

DEFINE_bool(log, true, "Enables logging, otherwise prints to stderr");
DEFINE_string(user_id,"0", "User ID");
DEFINE_string(group_id,"0", "Group ID");
DEFINE_string(pathFile,"", "File with local export Paths");

int main(int argc, char *argv[])
{
    const std::string version = WINNFSDPP_VERSION;

    gflags::SetUsageMessage("<flags> <export path> <alias path>");
    gflags::SetVersionString(version);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);


    wsa_session_t wsa_session(2, 2);
    program_t program;
    program.run();
}
