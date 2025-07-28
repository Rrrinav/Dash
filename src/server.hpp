#pragma once

#include <cstdint>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

constexpr uint16_t PORT = 9000;
constexpr const char * HOST = "127.0.0.1";

int init_server(uint16_t _port, const char *_host);

void run_server(int server_fd);
