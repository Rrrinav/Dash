#include "./src/server.hpp"

#include <csignal>
#include <cstdint>
#include <exception>
#include <print>
#include <string>

void handle_sigchld(int) { while (waitpid(-1, nullptr, WNOHANG) > 0); }

int print_usage(std::string prog)
{
  std::string s = "Usage : "
                  "   " + prog + "\n"
                  " or\n"
                  "   " + prog + "<port>\n";
  std::println("{}", s);
  return 1;
}

int main(int argc, char * argv[])
{
  uint16_t port = 0;
  if (argc == 2)
  {
    try
    { port = std::stoi(argv[1]); }
    catch(std::exception & e)
    { return print_usage(argv[0]); }
  }

  port = PORT;

  std::signal(SIGCHLD, handle_sigchld);

  int skt = init_server(port, HOST);
  run_server(skt);

  close(skt);
  return 0;
}
