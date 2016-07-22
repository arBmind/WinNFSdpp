#include "rpc_server.h"

#include "binary/binary.h"
#include "binary/binary_reader.h"
#include "binary/binary_builder.h"

#include "network/tcp.h"
#include "network/udp.h"

#include "rpc/rpc_router.h"

#include <thread>
#define GOOGLE_GLOG_DLL_DECL
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>

#include <string>
#include <map>

template<typename socket_t>
struct socket_thread_t {
  socket_thread_t() = default;

  socket_thread_t(const socket_thread_t&) = delete;
  socket_thread_t& operator= (const socket_thread_t&) = delete;

  socket_thread_t(socket_thread_t&& other) = default;
  socket_thread_t& operator= (socket_thread_t&& other) = default;

  ~socket_thread_t() { stop(); }

  static socket_thread_t with_socket(socket_t&& socket) {
    socket_thread_t result;
    result.socket = std::move(socket);
    return result;
  }

  template<typename callback_t>
  void start(callback_t&& callback) {
    if (!socket.valid()) socket = socket_t::create();
    thread = std::thread(std::forward<callback_t>(callback));
  }

  void stop() {
    if (socket.valid()) socket.close();
    if (thread.joinable()) thread.join();
  }

  socket_t socket;
  std::thread thread;
};
using udp_socket_thread_t = socket_thread_t<udp_socket_t>;
using tcp_socket_thread_t = socket_thread_t<tcp_socket_t>;

using tcp_session_map_t = std::map<std::string, tcp_socket_thread_t>;

struct rpc_server_t::impl {
  int port_m;
  rpc_router_t router_m;

  udp_socket_thread_t udp_socket_thread_m;
  tcp_socket_thread_t tcp_accept_socket_thread_m;

  tcp_session_map_t tcp_session_map_m;

public:
  impl(int port)
    : port_m(port)
  {}

  void add(const rpc_program_t &program) {
    router_m.add(program);
  }

  void start() {
    start_udp();
    start_tcp();
  }

  void start_udp() {
    udp_socket_thread_m.start([=] {
        udp_receive::config_t config;
        config.port = port_m;
        auto success = udp_receive::loop(udp_socket_thread_m.socket, config, [=](binary_t& binary, const inet_addr_t& remoteaddr) {
            router_args_t args;
            args.request_reader = binary_reader_t::binary(binary);
            args.sender = remoteaddr.name();
            auto result = router_m.handle(args);
            if ( !result.empty()) {
                udp_socket_thread_m.socket.send_to(result, remoteaddr);
              }
          });
        if ( !success) {
            LOG(FATAL) << "udp socket error " << WSAGetLastError() << std::endl;
            return;
          }
      });
  }

  void start_tcp() {
    tcp_accept_socket_thread_m.start([=] {
        tcp_accept::config_t config;
        config.backlog = 32;
        config.port = port_m;
        tcp_accept::loop(tcp_accept_socket_thread_m.socket, config, [=](tcp_socket_t&& socket, const inet_addr_t& remoteaddr) {
            start_tcp_session(std::move(socket), remoteaddr);
          });
      });
  }

  void start_tcp_session(tcp_socket_t&& socket, const inet_addr_t& remoteaddr) {
    auto sender = remoteaddr.name();
    auto it = tcp_session_map_m.find(sender);
    if (it != tcp_session_map_m.end()) {
        it->second.stop();
        it->second.socket = std::move(socket);
      }
    else {
        auto result = tcp_session_map_m.emplace(sender, tcp_socket_thread_t::with_socket(std::move(socket)));
        if (result.second) {
            it = result.first;
          }
        else return; // failed to insert
      }
    it->second.start([=] {
        tcp_receive::loop(it->second.socket, [=](binary_t& binary) {
            auto reader = binary_reader_t::binary(binary);
            auto message_size = reader.get32(0);
            if (0 == (message_size & 0x80000000)) return false; // drop connection
            message_size &= 0x7FFFFFFF;
            if (0xFFFFF < message_size) return false; // no single message should be >1MB
            if (binary.size() < 4 + message_size) return true; // need more data
            router_args_t args;
            args.request_reader = reader.get_reader(4, message_size);
            args.sender = it->first;
            auto result = router_m.handle(args);
            if ( !result.empty()) {
                binary_builder_t builder;
                builder.append32(0x80000000 | result.size());
                builder.append_binary(result);
                it->second.socket.send(builder.build());
              }
            binary.erase(binary.begin(), binary.begin() + 4 + message_size);
            return true; // keep connection
          });
      });
  }

};

rpc_server_t::rpc_server_t(int port)
  : p(new impl(port))
{}

rpc_server_t::~rpc_server_t()
{}

void rpc_server_t::add(const rpc_program_t &program)
{
  p->add(program);
}

void rpc_server_t::start()
{
  p->start();
}
