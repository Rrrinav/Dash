#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <coroutine>
#include <print>
#include <unordered_map>
#include <vector>
#include <queue>
#include <optional>

#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "data_tree.hpp"
#include "assert.hpp"
#include "./server.hpp"

std::unordered_map<int, std::coroutine_handle<>> FD_TO_HANDLE;

enum Event_type { READ, WRITE, ACCEPT };

int init_server(uint16_t _port, const char *_host)
{
  sockaddr_in sock{};
  int skt = socket(AF_INET, SOCK_STREAM, 0);
  __assert(skt != -1, std::format("Socket initialization failed: {}", std::strerror(errno)));

  int opt = 1;
  __assert(setsockopt(skt, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0, std::format("setsockopt failed: {}", std::strerror(errno)));

  sock.sin_family = AF_INET;
  sock.sin_port = htons(_port);
  sock.sin_addr.s_addr = inet_addr(_host);

  __assert(bind(skt, (struct sockaddr *)&sock, sizeof(sock)) == 0, std::format("Socket binding failed: {}", std::strerror(errno)));
  __assert(listen(skt, 20) == 0, std::format("Socket listening failed: {}", std::strerror(errno)));

  std::println("Listening on {}:{}", _host, _port);
  return skt;
}

inline void add_to_epoll(int epfd, int fd, uint32_t events, void *ptr)
{
  epoll_event ev{};
  ev.events = events;
  ev.data.ptr = ptr;

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1)
  {
    if (errno == EEXIST)
    {
      // Modify instead if already exists
      if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1)
      {
        std::println("Modifying socket: {} in epoll failed: {}", fd, std::strerror(errno));
        std::exit(1);
      }
    }
    else
    {
      std::println("Adding socket: {} to epoll failed: {}", fd, std::strerror(errno));
      std::exit(1);
    }
  }
}

void set_non_blocking(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  __assert(flags != -1, std::format("fcntl get failed: {}", std::strerror(errno)));
  __assert(fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1, std::format("fcntl set O_NONBLOCK failed: {}", std::strerror(errno)));
}

struct Async_task
{
  struct promise_type
  {
    bool completed =  false;
    Async_task get_return_object() { return Async_task{std::coroutine_handle<promise_type>::from_promise(*this)}; }

    std::suspend_never initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    void return_void() { }// completed = true; }
    void unhandled_exception() { std::terminate(); }
  };

  std::coroutine_handle<promise_type> handle;

  explicit Async_task(std::coroutine_handle<promise_type> h) : handle(h) {}

  Async_task(Async_task &&other) noexcept : handle(other.handle) { other.handle = nullptr; }

  Async_task &operator=(Async_task &&other) noexcept
  {
    if (this != &other)
    {
      if (handle)
        handle.destroy();
      handle = other.handle;
      other.handle = nullptr;
    }
    return *this;
  }

  Async_task(const Async_task &) = delete;
  Async_task &operator=(const Async_task &) = delete;

  ~Async_task()
  {
    if (handle && !handle.done())
    {
      handle.destroy();
    }
  }

  void resume() const
  {
    if (handle && !handle.done())
      handle.resume();
  }

  bool done() const { return handle == nullptr || handle.done() || handle.promise().completed; }
};

struct Send_task
{
  struct promise_type
  {
    bool completed =  false;
    Send_task get_return_object() { return Send_task{std::coroutine_handle<promise_type>::from_promise(*this)}; }

    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }

    void return_void() { }// completed = true; }
    void unhandled_exception() { std::terminate(); }
  };

  std::coroutine_handle<promise_type> handle;

  explicit Send_task(std::coroutine_handle<promise_type> h) : handle(h) {}

  Send_task(Send_task &&other) noexcept : handle(other.handle) { other.handle = nullptr; }

  Send_task &operator=(Send_task &&other) noexcept
  {
    if (this != &other)
    {
      if (handle)
        handle.destroy();
      handle = other.handle;
      other.handle = nullptr;
    }
    return *this;
  }

  Send_task(const Send_task &) = delete;
  Send_task &operator=(const Send_task &) = delete;

  ~Send_task()
  {
    if (handle && !handle.done())
    {
      handle.destroy();
    }
  }

  void resume() const
  {
    if (handle && !handle.done())
      handle.resume();
  }

  bool done() const { return handle == nullptr || handle.done() || handle.promise().completed; }
};

struct Client
{
  int fd;
  Tree tree = Tree{};
  int epfd;
  uint16_t port;
  std::string addr;
  bool is_sending = false;
  std::queue<std::string> send_queue;
  bool is_alive = true;

  // Current send state
  std::string current_send_data;
  size_t current_send_offset = 0;

  Async_task * read_task;
  std::optional<Send_task> send_task;

  ~Client()
  { __assert(read_task == nullptr, "Must clear the read task before deleting client."); }

  void queue_send(const std::string &data)
  {
    send_queue.push(data);
    if (!is_sending && is_alive)
      try_start_next_send();
  }

  void try_start_next_send();
  bool try_send_current();
};

// Global map to track clients by fd
std::unordered_map<int, Client *> CLIENTS;

struct Accept_awaitable
{
  int listen_fd;
  int epfd;
  Event_type event_type = Event_type::ACCEPT;
  std::coroutine_handle<> handle = nullptr;

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<> h) { handle = h; }

  Client await_resume()
  {
    sockaddr_in client_addr{};
    socklen_t addrlen = sizeof(client_addr);
    int client_fd = accept(listen_fd, (sockaddr *)&client_addr, &addrlen);
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, sizeof(ip_str));
    int port = ntohs(client_addr.sin_port);
    __assert(client_fd != -1, std::format("Failed to accept from : {} : {}", client_fd, std::strerror(errno)));

    set_non_blocking(client_fd);
    return Client{client_fd, {}, epfd, (uint16_t)port, ip_str};
  }
};

struct Recv_awaitable
{
  Client *client;
  Event_type event_type = Event_type::READ;
  std::coroutine_handle<> handle;

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<> h)
  {
    handle = h;

    if (!client || !client->is_alive)
      return;

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.ptr = this;

    // Register client fd to epoll
    if (epoll_ctl(client->epfd, EPOLL_CTL_MOD, client->fd, &ev) == -1)
    {
      std::println("epoll_ctl failed: {}", std::strerror(errno));
      if (client)
        client->is_alive = false;
      return;
    }

    FD_TO_HANDLE[client->fd] = handle;
  }

  std::string await_resume()
  {
    if (!client || !client->is_alive)
      return {};

    // Try reading from client
    char buf[1024];
    ssize_t n = recv(client->fd, buf, sizeof(buf), 0);
    if (n <= 0)
    {
      if (client)
        client->is_alive = false;
      return {};
    }

    return std::string(buf, n);
  }

  void resume()
  {
    if (handle && !handle.done())
      handle.resume();
  }
};

struct Send_awaitable
{
  Client *client;
  Event_type event_type = Event_type::WRITE;
  std::coroutine_handle<> handle;

  explicit Send_awaitable(Client *c) : client(c) {}

  bool await_ready() const noexcept
  {
    // Try to send immediately
    if (!client || !client->is_alive)
      return true;
    return const_cast<Send_awaitable *>(this)->try_send_now();
  }

  bool try_send_now()
  {
    if (!client || !client->is_alive)
      return true;

    return client->try_send_current();
  }

  void await_suspend(std::coroutine_handle<> h)
  {
    handle = h;

    if (!client || !client->is_alive)
      return;

    epoll_event ev = {};
    ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLET;
    ev.data.ptr = this;

    if (epoll_ctl(client->epfd, EPOLL_CTL_MOD, client->fd, &ev) == -1)
    {
      std::println("epoll_ctl (send) failed: {}", std::strerror(errno));
      if (client)
        client->is_alive = false;
      return;
    }
  }

  bool await_resume()
  {
    if (!client || !client->is_alive)
      return true;

    return client->try_send_current();
  }

  void resume()
  {
    if (handle && !handle.done())
      handle.resume();
  }
};

bool Client::try_send_current()
{
  if (!is_alive || current_send_data.empty())
    return true;

  while (current_send_offset < current_send_data.length())
  {
    ssize_t n = ::send(fd, current_send_data.data() + current_send_offset, current_send_data.length() - current_send_offset, 0);
    if (n == -1)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        // Need to wait for EPOLLOUT
        return false;
      }
      else
      {
        std::println("Send error: {}", std::strerror(errno));
        is_alive = false;
        return true;
      }
    }
    current_send_offset += n;
  }

  // Current message sent completely
  current_send_data.clear();
  current_send_offset = 0;
  return true;
}

Send_task send_handler(Client *client)
{
  while (client && client->is_alive && client->is_sending)
  {
    Send_awaitable sender(client);
    bool complete = co_await sender;

    if (!client || !client->is_alive)
      break;

    if (complete)
    {
      // Current message done, try next
      client->is_sending = false;
      if (!client->send_queue.empty())
        client->try_start_next_send();
      break;
    }
    // If not complete, continue the loop to wait for more EPOLLOUT
  }

  if (client) {
    client->is_sending = false;
    client->send_task.reset(); // Clear the task when done
  }
  co_return;
}

void Client::try_start_next_send()
{
  if (is_sending || !is_alive || send_queue.empty())
    return;

  // Get next message
  current_send_data = send_queue.front();
  send_queue.pop();
  current_send_offset = 0;
  is_sending = true;

  // Try immediate send
  if (try_send_current())
  {
    // Sent completely, check for more
    is_sending = false;
    if (!send_queue.empty())
      try_start_next_send();
  }
  else
  {
    // Need to wait for EPOLLOUT, start send handler coroutine
    send_task.emplace(send_handler(this));
    send_task->resume();
  }
}

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

void process_command(Client *client, const std::string &data)
{
  if (!client || !client->is_alive)
    return;

  auto cmd = parse_command(data);
  if (!cmd)
  {
    client->queue_send("Bad command\r\n");
    return;
  }

  switch (cmd->_type)
  {
    case Query_type::HELP:
    {
      std::string mess =
          "Commands:\r\n"
          "  create <path>\r\n"
          "  put <path> <key> <value>\r\n"
          "  get <path> <key>\r\n";
      client->queue_send(mess);
      break;
    }
    case Query_type::SHOW:
    {
      std::string mess = client->tree.print();
      client->queue_send(mess);
      break;
    }
    case Query_type::CREATE:
    {
      client->tree.insert(cmd->_path);
      client->queue_send("100 OK\r\n");
      break;
    }
    case Query_type::GET:
    {
      auto s = client->tree.get(cmd->_path, cmd->_key);
      std::string mess = s ? *s.value() + "\r\n" : s.error();
      client->queue_send(mess);
      break;
    }
    case Query_type::PUT:
    {
      auto s = client->tree.set(cmd->_path, cmd->_key, cmd->_value);
      std::string mess = s ? "100 OK\r\n" : s.error();
      client->queue_send(mess);
      break;
    }
    case Query_type::INVALID:
    default:
    {
      client->queue_send("Invalid command\r\n");
      break;
    }
  };
}

void cleanup_client(Client *client)
{
  if (!client)
    return;

  int fd = client->fd;

  // Check if already cleaned up
  if (CLIENTS.find(fd) == CLIENTS.end() || CLIENTS[fd] != client)
    return;  // Already cleaned up

  client->is_alive = false;

  // Clean up epoll and close socket
  FD_TO_HANDLE.erase(fd);
  epoll_ctl(client->epfd, EPOLL_CTL_DEL, fd, nullptr);
  close(fd);

  // Remove from global map first
  CLIENTS.erase(fd);
  delete client;
}

Async_task client_read(Client *client)
{
  while (client && client->is_alive)
  {
    std::string data = co_await Recv_awaitable{client};
    if (data.empty())
    {
      std::print("Client {} disconnected or read error\n", client->fd);
      // Client will be cleaned up when we exit this function
      break;
    }
    std::print("{}", data);

    // Process command synchronously to avoid race conditions
    process_command(client, data);
  }
  co_return;
}

Async_task accept_clients(int epfd, int listen_fd)
{
  Accept_awaitable acceptor = Accept_awaitable{listen_fd, epfd};
  add_to_epoll(epfd, listen_fd, EPOLLIN | EPOLLET, &acceptor);

  while (true)
  {
    auto client_data = co_await acceptor;

    std::println("Accepted new client: {}:{}, fd={}", client_data.addr, client_data.port, client_data.fd);

    // Create client on heap properly
    Client *client = new Client{.fd = client_data.fd,
                                .tree = {},
                                .epfd = epfd,
                                .port = client_data.port,
                                .addr = client_data.addr,
                                .is_sending = false,
                                .send_queue = {},
                                .is_alive = true,
                                .current_send_data = {},
                                .current_send_offset = 0,
                                .send_task = std::nullopt };
    CLIENTS[client->fd] = client;

    std::string mess = "100 connected Ok\r\n";
    send(client->fd, mess.data(), mess.length(), 0);

    add_to_epoll(epfd, client->fd, EPOLLIN | EPOLLET | EPOLLONESHOT, nullptr);
    client->read_task = new Async_task(client_read(client));
  }
  co_return;
}

void run_server(int server_fd)
{
  int epfd = epoll_create1(0);
  __assert(epfd != -1, std::format("epoll_create1 failed: {}", std::strerror(errno)));

  static Async_task accept_coroutine = accept_clients(epfd, server_fd);
  accept_coroutine.resume();

  constexpr int MAX_EVENTS = 64;
  epoll_event events[MAX_EVENTS];

  while (true)
  {
    int num_events = epoll_wait(epfd, events, MAX_EVENTS, -1);
    __assert(num_events >= 0, std::format("epoll_wait failed: {}", std::strerror(errno)));

    for (int i = 0; i < num_events; ++i)
    {
      void *ptr = events[i].data.ptr;

      if (ptr == nullptr)
        continue;

      // Check if this is an Accept_awaitable
      Accept_awaitable *accept = static_cast<Accept_awaitable *>(ptr);
      if (accept->event_type == Event_type::ACCEPT && accept->listen_fd == server_fd)
      {
        FD_TO_HANDLE[accept->listen_fd] = accept->handle;
        accept->handle.resume();
        continue;
      }

      // Check for read events
      if (auto *recv = static_cast<Recv_awaitable *>(ptr); recv->event_type == Event_type::READ)
      {
        // Check if client still exists
        if (recv->client && CLIENTS.count(recv->client->fd) && CLIENTS[recv->client->fd] == recv->client)
          recv->resume();
        if (recv->handle.done())
        {
          std::println("Cleaning reading task and client: {}:{} fd= {}.", recv->client->addr, recv->client->port, recv->client->fd);
          if (recv->client->read_task) delete recv->client->read_task;
          recv->client->read_task = nullptr;
          cleanup_client(recv->client);
        }
        continue;
      }

      // Check for write events
      if (auto *send_awaitable = static_cast<Send_awaitable *>(ptr); send_awaitable->event_type == Event_type::WRITE)
      {
        // Check if client still exists
        if (send_awaitable->client && CLIENTS.count(send_awaitable->client->fd) &&
            CLIENTS[send_awaitable->client->fd] == send_awaitable->client)
        {
          send_awaitable->resume();
        }
        continue;
      }

    }
  }
}
