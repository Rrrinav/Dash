# DASH

> Cached database

A simple in-memory cached database

Main focus of this DB is to increase CPU speed/efficiency and not the network speed/efficiency or memory efficiency.
So beware of the trade-offs.

## Usage

```bash
$ make -B
./dist/main <port> # will choose default port 9000 if you dont provide any.
```
---

**Connect to server form client, e.g. using telnet**

```bash
telnet <ip> <port>
> 100 connected OK  #response
$ create users
> 100 OK
$ create users/login
> 100 OK
$ put users/login john 3
> 100 OK
$ get users/login john
> 3

```
