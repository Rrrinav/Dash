#include "./server.hpp"
#include <cstddef>
#include <optional>
#include <string>

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <utility>
#include <cstring>
#include <format>
#include <print>

#include "./data_tree.hpp"
#include "./assert.hpp"

enum class Query_type { GET, PUT, CREATE, HELP, DEL, SHOW, INVALID };

struct Query
{
  Query_type _type;
  std::string _path;
  std::string _key;
  std::string _value;
};

inline void trim_left(std::string &str)
{
  size_t s = str.find_first_not_of(" \t");
  if (s == std::string::npos)
    str.clear();
  else
    str.erase(0, s);
}

inline void trim_right(std::string &str)
{
  size_t pos = str.find_last_not_of(" \r\n\t");
  if (pos != std::string::npos)
    str.erase(pos + 1);
  else
    str.clear();
}

std::vector<std::string> split_by_space(std::string input)
{
  std::vector<std::string> tokens;
  trim_left(input);

  while (!input.empty())
  {
    size_t space_pos = input.find(' ');
    if (space_pos == std::string::npos)
    {
      tokens.push_back(input);
      break;
    }
    else
    {
      tokens.push_back(input.substr(0, space_pos));
      input.erase(0, space_pos + 1);
      trim_left(input);
    }
  }
  return tokens;
}

std::optional<Query> parse_command(std::string input)
{
  trim_right(input);
  trim_left(input);

  if (input.empty())
    return std::nullopt;

  // Handle help command first
  if (input == "help" || input == "-h" || input == "Help" || input == "h")
    return Query{Query_type::HELP, {}, {}, {}};

  if (input == "show" || input == "-p" || input == "print" || input == "Print" || input == "Show")
    return Query{Query_type::SHOW, {}, {}, {}};

  auto tokens = split_by_space(input);
  if (tokens.empty())
    return std::nullopt;

  Query result{Query_type::INVALID, {}, {}, {}};
  const auto &cmd = tokens[0];

  if (cmd == "create")
  {
    if (tokens.size() < 2)
      return std::nullopt;
    result._type = Query_type::CREATE;
    result._path = tokens[1];
    return result;
  }

  if (cmd == "get")
  {
    if (tokens.size() < 3)
      return std::nullopt;
    result._type = Query_type::GET;
    result._path = tokens[1];
    result._key = tokens[2];
    return result;
  }

  if (cmd == "put")
  {
    if (tokens.size() < 4)
      return std::nullopt;
    result._type = Query_type::PUT;
    result._path = tokens[1];
    result._key = tokens[2];
    result._value = tokens[3];
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

void child_routine(int s_cli, std::unique_ptr<Sk_client> &client, Tree &tree, bool &should_stop)
{
  std::string buffer(255, '\0');
  int bytes_read = read(s_cli, buffer.data(), 254);

  if (bytes_read < 0)
  {
    std::println(stderr, "Read error from client: {}:{} : [error]: {}", client->ip, client->port, std::strerror(errno));
    should_stop = true;
    return;
  }
  if (0 == bytes_read)
  {
    std::println("Client disconnected");
    should_stop = true;
    return;
  }

  buffer.resize(bytes_read);
  std::print("Received: {}", buffer);

  auto cmd = parse_command(buffer);
  if (!cmd)
  {
    std::string mess = "Bad command\r\n";
    send(s_cli, mess.data(), mess.size(), 0);
    return;
  }

  switch (cmd->_type)
  {
    case Query_type::HELP: {
      std::string mess =
          "Commands:\r\n"
          "  create <path>\r\n"
          "  put <path> <key> <value>\r\n"
          "  get <path> <key>\r\n";

      send(s_cli, mess.data(), mess.size(), 0);
    } break;
    case Query_type::SHOW: {
      std::string mess = tree.print();
      send(s_cli, mess.data(), mess.size(), 0);
    } break;
    case Query_type::CREATE: {
      auto n = tree.insert(cmd->_path);
      if (n == nullptr)
        std::println("null");

      std::string mess = "100 OK\r\n";
      send(s_cli, mess.data(), mess.size(), 0);
    } break;
    case Query_type::GET: {
      auto s = tree.get(cmd->_path, cmd->_key);
      if (s)
      {
        std::string mess = *s.value() + "\r\n";
        send(s_cli, mess.data(), mess.size(), 0);
      }
      else
      {
        std::string error_msg = s.error();
        send(s_cli, error_msg.data(), error_msg.size(), 0);
      }
    } break;
    case Query_type::PUT: {
      auto s = tree.set(cmd->_path, cmd->_key, cmd->_value);
      if (!s)
      {
        std::string error_msg = s.error();
        send(s_cli, error_msg.data(), error_msg.size(), 0);
      }
      else
      {
        std::string mess = "100 OK\r\n";
        send(s_cli, mess.data(), mess.size(), 0);
      }
    } break;
    case Query_type::INVALID: default: {
      std::string mess = "Invalid command\r\n";
      send(s_cli, mess.data(), mess.size(), 0);
    } break;
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
  auto client =
      std::make_unique<Sk_client>(Sk_client{.skt = s_cli, .ip = inet_ntoa(client_addr.sin_addr), .port = ntohs(client_addr.sin_port)});

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
    Tree tree;

    while (!should_child_stop)
      child_routine(s_cli, client, tree, should_child_stop);

    close(s_cli);
    _exit(0);
  }
  else  // Fork failed
  {
    std::perror("fork failed");
  }

  std::unreachable();
}
