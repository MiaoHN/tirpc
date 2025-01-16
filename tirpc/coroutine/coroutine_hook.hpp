#pragma once

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using read_fun_ptr_t = ssize_t (*)(int, void *, size_t);

using write_fun_ptr_t = ssize_t (*)(int, const void *, size_t);

using connect_fun_ptr_t = int (*)(int, const struct sockaddr *, __socklen_t);

using accept_fun_ptr_t = int (*)(int, struct sockaddr *, __socklen_t *);

using socket_fun_ptr_t = int (*)(int, int, int);

using sleep_fun_ptr_t = int (*)(unsigned int);

namespace tirpc {

int accept_hook(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

ssize_t read_hook(int fd, void *buf, size_t count);

ssize_t write_hook(int fd, const void *buf, size_t count);

int connect_hook(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

unsigned int sleep_hook(unsigned int seconds);

void SetHook(bool);

}  // namespace tirpc

extern "C" {

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

ssize_t read(int fd, void *buf, size_t count);

ssize_t write(int fd, const void *buf, size_t count);

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

unsigned int sleep(unsigned int seconds);
}
