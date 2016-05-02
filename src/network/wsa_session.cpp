#include "wsa_session.h"

#include <WinSock2.h>

/*
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")
*/

wsa_session_t::wsa_session_t(int major, int minor)
  : activated_m(true)
{
  WSAData wsaData; // move out if needed
  WSAStartup(MAKEWORD(major, minor), &wsaData);
}

wsa_session_t::~wsa_session_t()
{
  if (activated_m) {
      WSACleanup();
    }
}
