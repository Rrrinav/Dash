#include "./server.hpp"

#include <cerrno>
#include <utility>
#include <cstring>
#include <format>
#include <print>
#include <csignal>

#include "./assert.hpp"

void handle_sigchld(int) { while (waitpid(-1, nullptr, WNOHANG) > 0); }

int init_server(uint16_t _port)
{
  sockaddr_in sock{};
  int skt = socket(AF_INET, SOCK_STREAM, 0);
  __assert(skt != -1, std::format("Socket initialization failed: {}", std::strerror(errno)));

  // TODO: Add timeouts
  int opt = 1;
  __assert(setsockopt(skt, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0, std::format("setsockopt failed: {}", std::strerror(errno)));

  sock.sin_family = AF_INET;
  sock.sin_port = htons(_port);
  sock.sin_addr.s_addr = inet_addr(HOST);

  __assert(bind(skt, (struct sockaddr *)&sock, sizeof(sock)) == 0, std::format("Socket binding failed: {}", std::strerror(errno)));

  __assert(listen(skt, 20) == 0, std::format("Socket listening failed: {}", std::strerror(errno)));

  std::println("Listening on {}:{}", HOST, PORT);
  return skt;
}

void child_routine(int s_cli, std::unique_ptr<Sk_client> &client, bool &should_stop)
{
}

void main_routine(int _skt)
{
  sockaddr_in client_addr{};
  socklen_t len = sizeof(client_addr);

  int s_cli = accept(_skt, (struct sockaddr *)&client_addr, &len);
  if (s_cli < 0)
    return;

  // PERF: This is slow, we can use memory pool or any other method
  auto client = std::make_unique<Sk_client>(Sk_client{
    .skt = s_cli,
    .ip = inet_ntoa(client_addr.sin_addr),
    .port = ntohs(client_addr.sin_port)
  });

  std::println("Accepted := {}:{}", client->ip, client->port);

  // TODO: Forking is not efficient, change it in future but use for current purposes.
  int pid = fork();

  if (pid > 0)  // Parent
  {
    close(s_cli);
    return;
  }
  else if (pid == 0)  // Child
  {
    bool should_child_stop = false;

    while (!should_child_stop)
      child_routine(s_cli, client, should_child_stop);

    close(s_cli);
    _exit(0);
  }
  else  // Fork failed
  {
    std::perror("fork failed");
  }

  std::unreachable();
}

int main()
{
  std::signal(SIGCHLD, handle_sigchld);

  int skt = init_server(PORT);

  while (true)
    main_routine(skt);

  close(skt);
  return 0;
}
