#pragma once

#include "inet.h"
#include "binary/binary.h"

#include <cassert>
#include <utility>
#include <algorithm>

#include <winsock2.h>
#undef max

struct tcp_socket_t
{
  tcp_socket_t() = default;
  ~tcp_socket_t() {
    if (valid()) {
        closesocket(handle_m);
      }
  }

  tcp_socket_t(const tcp_socket_t&) = delete;
  tcp_socket_t& operator= (const tcp_socket_t&) = delete;

  tcp_socket_t(tcp_socket_t&& other) {
    handle_m = other.handle_m;
    other.handle_m = INVALID_SOCKET;
  }
  tcp_socket_t& operator= (tcp_socket_t&& other) {
    using namespace std;
    swap(handle_m, other.handle_m);
    return *this;
  }

  static tcp_socket_t create() {
    tcp_socket_t result;
    result.handle_m = ::socket(AF_INET, SOCK_STREAM, 0);
    return result;
  }

  void close() {
    closesocket(handle_m);
    handle_m = INVALID_SOCKET;
  }

  bool valid() const { return handle_m != INVALID_SOCKET; }

  int bind_all(int port) const {
    assert(valid());
    auto localAddr = inet_addr_t::any(port);
    return ::bind(handle_m, reinterpret_cast<const sockaddr*>(&localAddr), sizeof(localAddr));
  }

  int listen(int backlog = 32) const {
    assert(valid());
    return ::listen(handle_m, backlog);
  }

  tcp_socket_t accept(sockaddr_in& remoteaddr) const {
    assert(valid());
    tcp_socket_t result;
    int addrlen = sizeof(remoteaddr);
    result.handle_m = ::accept(handle_m, reinterpret_cast<sockaddr*>(&remoteaddr), &addrlen);
    return result;
  }

  int send(const binary_t& buffer) const {
    assert(valid());
    return ::send(handle_m, (char*)&buffer[0], buffer.size(), 0);
  }

  int receive(binary_t& buffer) const {
    assert(valid());
    auto offset = buffer.size();
    buffer.resize(buffer.size() + 1500);
    auto result = ::recv(handle_m, (char*)&buffer[offset], buffer.size() - offset, 0);
    buffer.resize(offset + std::max(0, result));
    return result;
  }

private:
  SOCKET handle_m = INVALID_SOCKET;
};

struct tcp_accept
{
  struct config_t {
    int port;
    int backlog;
  };
  //using callback_t = std::function<void (socket_t&&, inet_addr_t&&)>;

  template<typename callback_t>
  static bool loop(const tcp_socket_t& socket, const config_t& config, callback_t&& callback) {
    if ( !socket.valid()) return false;
    const auto bind_result = socket.bind_all(config.port);
    if (bind_result == SOCKET_ERROR) return false;
    const auto listen_result = socket.listen(config.backlog);
    if (listen_result == SOCKET_ERROR) return false;
    while (true) {
        inet_addr_t remote_addr;
        auto accepted_socket = socket.accept(remote_addr);
        if (accepted_socket.valid()) {
          callback(std::move(accepted_socket), std::move(remote_addr));
        }
        else break;
    }
    return true;
  }
};

struct tcp_receive
{
  template<typename callback_t>
  static bool loop(const tcp_socket_t& socket, callback_t&& callback) {
    if ( !socket.valid()) return false;
    binary_t buffer;
    buffer.reserve(4096);
    while (true) {
        auto bytes = socket.receive(buffer);
        if (bytes > 0) {
            bool keep = callback(buffer);
            if ( !keep) break;
          }
        else break;
    }
    return true;
  }
};
