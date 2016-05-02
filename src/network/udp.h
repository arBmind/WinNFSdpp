#pragma once

#include "inet.h"
#include "binary/binary.h"

#include <cassert>
#include <algorithm>

#include <winsock2.h>
#undef max

struct udp_socket_t
{
  udp_socket_t() = default;
  ~udp_socket_t() {
    if (valid()) {
      closesocket(handle_m);
    }
  }

  udp_socket_t(const udp_socket_t&) = delete;
  udp_socket_t& operator= (const udp_socket_t&) = delete;

  udp_socket_t(udp_socket_t&& other) {
    handle_m = other.handle_m;
    other.handle_m = INVALID_SOCKET;
  }
  udp_socket_t& operator= (udp_socket_t&& other) {
    using namespace std;
    swap(handle_m, other.handle_m);
    return *this;
  }

  void close() {
    closesocket(handle_m);
    handle_m = INVALID_SOCKET;
  }

  static udp_socket_t create() {
    udp_socket_t result;
    result.handle_m = ::socket(AF_INET, SOCK_DGRAM, 0);
    return std::move(result);
  }

  bool valid() const { return handle_m != INVALID_SOCKET; }

  int bind_all(int port) const {
    assert(valid());
    auto localAddr = inet_addr_t::any(port);
    return ::bind(handle_m, reinterpret_cast<const sockaddr*>(&localAddr), sizeof(localAddr));
  }

  int send_to(binary_t& buffer, const inet_addr_t& addr) const {
    assert(valid());
    return ::sendto(handle_m, (char*)&buffer[0], buffer.size(), 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
  }

  int receive_from(binary_t& buffer, inet_addr_t& remoteaddr) const {
    assert(valid());
    int addrlen = sizeof(remoteaddr);
    buffer.resize(buffer.capacity());
    auto result = ::recvfrom(handle_m, (char*)&buffer[0], buffer.size(), 0, reinterpret_cast<sockaddr*>(&remoteaddr), &addrlen);
    buffer.resize(std::max(0, result));
    return result;
  }

private:
  SOCKET handle_m = INVALID_SOCKET;
};

struct udp_receive {
  struct config_t {
    int port;
  };
  //using callback_t = std::function<void (binary_t& buffer, const inet_addr_t&)>;

  template<typename callback_t>
  static bool loop(const udp_socket_t& socket, const config_t& config, callback_t&& callback) {
    if ( !socket.valid()) return false;
    const auto bind_result = socket.bind_all(config.port);
    if (bind_result == SOCKET_ERROR) return false;
    binary_t buffer;
    buffer.resize(2000);
    while (socket.valid()) {
        inet_addr_t remote_addr;
        int bytes = socket.receive_from(buffer, remote_addr);
        if (bytes > 0) {
            callback(buffer, remote_addr);
            buffer.clear();
          }
        else break;
      }
    return true;
  }
};
