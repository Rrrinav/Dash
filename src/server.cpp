#include "./server.hpp"
#include <cstddef>
#include <optional>
#include <string_view>

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <utility>
#include <cstring>
#include <format>
#include <print>
#include <csignal>

#include "./assert.hpp"

enum class Query_type { GET, PUT, CREATE, HELP, INVALID };

struct Query
{
  Query_type       _type;
  std::string_view _path;
  std::string_view _key;
  std::string_view _value;
};

void handle_sigchld(int) { while (waitpid(-1, nullptr, WNOHANG) > 0); }

inline void trim_left(std::string_view &str)
{
  int s = str.find_first_not_of(" \t");
  str.remove_prefix(s == std::string::npos ? str.size() : s);
}

inline void trim_right(std::string_view& str)
{
  size_t pos = str.find_last_not_of(" \r\n\t");
  if (pos != std::string_view::npos)
    str.remove_suffix(str.size() - pos - 1);
  else
    str = {}; // All whitespace
}

std::optional<Query> parse_command(std::string_view input)
{
  trim_right(input);
  trim_left(input);

  if (input == "help" || input == "-h" || input == "Help" || input == "h")
    return Query {Query_type::HELP, {}, {}, {}};

  size_t space1 = input.find(' ');
  if (space1 == std::string_view::npos)
    return std::nullopt;

  std::string_view cmd = input.substr(0, space1);
  input.remove_prefix(space1 + 1);
  trim_left(input);

  Query result{Query_type::INVALID, {}, {}, {}};

  if (cmd == "create")
  {
    result._type = Query_type::CREATE;
    result._path = input;
    return result;
  }

  size_t space2 = input.find(' ');
  if (space2 == std::string_view::npos)
    return std::nullopt;

  std::string_view arg1 = input.substr(0, space2);  // path
  input.remove_prefix(space2 + 1);
  trim_left(input);

  if (cmd == "get")
  {
    result._type = Query_type::GET;
    result._path = arg1;
    result._key  = input;
    return result;
  }

  size_t space3 = input.find(' ');
  if (space3 == std::string_view::npos)
    return std::nullopt;

  std::string_view arg2 = input.substr(0, space3);
  std::string_view arg3 = input.substr(space3 + 1);

  if (cmd == "put")
  {
    result._type  = Query_type::PUT;
    result._path  = arg1;
    result._key   = arg2;
    result._value = arg3;
    return result;
  }

  return std::nullopt;
}

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
  std::string buffer(255, '\0');;
  int bytes_read = read(s_cli, buffer.data(), 254);

  if (bytes_read < 0)
  {
    std::println(stderr, "Read error from client: {}:{} : [error]: {}", client->ip, client->port, std::strerror(errno));
    should_stop = true;
    return;
  }
  if (0 == bytes_read)
  {
    std::println("CLient disconnected");
    should_stop = true;
    return;
  }

  buffer.resize(bytes_read);
  std::println("Received: {}", buffer);

  auto cmd = parse_command(buffer);
  if (!cmd)
  {
    std::string mess = "Bad command\r\n"; 
    send(s_cli, mess.data(), mess.size(),  0);
    return;                               
  }
  switch (cmd->_type)
  {
    case Query_type::HELP: {
      std::string mess = "Commands\r\n"
                         "  create <path>\r\n"
                         "  put <path> <key> <value>\r\n"
                         "  get <path> <key>\r\n";

      send(s_cli, mess.data(), mess.size(), 0);
    } break;
    case Query_type::CREATE: {
      std::string mess = std::format("Creating: {}\r\n", cmd->_path);
      send(s_cli, mess.data(), mess.size(), 0);
    } break;
    case Query_type::GET: {
      std::string mess = std::format("Sending value of {} at path: {}\r\n", cmd->_key, cmd->_path);
      send(s_cli, mess.data(), mess.size(), 0);
    } break;
    case Query_type::PUT: {
      std::string mess = std::format("Setting value of {} as {} at path: {}\r\n", cmd->_key, cmd->_value, cmd->_path);
      send(s_cli, mess.data(), mess.size(), 0);
    } break;
    case Query_type::INVALID: default: {
      std::string mess = "Invalid command\r\n";
      send(s_cli, mess.data(), mess.size(), 0);
    }break;
  };
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

    std::string mess = "200 Connected\r\n";
    send(s_cli, mess.data(), mess.size(), 0);

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
