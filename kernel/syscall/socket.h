#pragma once
#include <stdint.h>

struct sockaddr_un {
    uint16_t sun_family;
    char sun_path[108];
};

int64_t sys_socket_connect(int fd, struct sockaddr_un* addr, uint64_t addrlen);
int64_t sys_socket_accept(int fd, struct sockaddr_un* addr, int* addrlen, int flags);
int64_t sys_socket_bind(int fd, struct sockaddr_un* addr, uint64_t addrlen);
int64_t sys_socket_getsockname(int fd, struct sockaddr_un* addr, int* addrlen);
int64_t sys_socket_getpeername(int fd, struct sockaddr_un* addr, int* addrlen);
int64_t sys_socket_setsockopt(int fd, int level, int opt, void* val, int optlen);
int64_t sys_socket_getsockopt(int fd, int level, int opt, void* val, int* optlen);
int64_t sys_socket_sendmsg(int fd, const void* mhdr, int flags);
int64_t sys_socket_recvmsg(int fd, void* mhdr, int flags);
