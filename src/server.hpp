#pragma once

#include <cstdint>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory>

constexpr unsigned short PORT = 9000;
constexpr const char * HOST = "127.0.0.1";

struct Sk_client
{
  int skt;
  std::string ip;
  uint16_t port;
};

int init_server(uint16_t _port);

void child_routine(int s_cli, std::unique_ptr<Sk_client> &client, bool &should_stop);

void main_routine(int _skt);
