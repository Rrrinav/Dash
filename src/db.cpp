#include "./db.hpp"

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <format>

#include "./assert.hpp"

int init_server(uint16_t _port)
{
  struct sockaddr_in sock;
  int skt;

  sock.sin_family      = AF_INET;
  sock.sin_port        = htons(_port);
  sock.sin_addr.s_addr = inet_addr(HOST);

  skt = socket(AF_INET, SOCK_STREAM, 0);
  __assert((-1 != skt), std::format("Socket initialization failed: {}.", std::strerror(errno)));

  errno = 0;
  __assert((0 == bind(skt, (struct sockaddr *)&sock, sizeof(sock))), std::format("Socket binding failed: {}.", std::strerror(errno)));

  errno = 0;
  __assert(!listen(skt, 20), std::format("Socket listening failed: {}.", std::strerror(errno)));;
  std::println("Listening to {}:{}", HOST, PORT);

  return skt;
}

void main_routine(int _skt)
{
}

int main(int argc, char *argv[])
{
  bool stop_server = false;

  int skt = init_server(PORT);

  while (!stop_server)

  return 0;
}
