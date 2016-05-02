#pragma once

#include <winsock2.h>
#include <string>

struct inet_addr_t : sockaddr_in {
  inet_addr_t() {
    memset(this, 0, sizeof(sockaddr_in));
    sin_family = AF_INET;
  }

  int port() const { return ntohs(sin_port); }

  std::string ip() const { return inet_ntoa(sin_addr); }

  std::string name() const { return ip() + ":" + std::to_string(port()); }

  static inet_addr_t any(int port) {
    inet_addr_t result;
    result.sin_port = htons(port);
    result.sin_addr.s_addr = INADDR_ANY;
    return result;
  }
};
