#pragma once
#include <stdint.h>

int fd_ioctl(int fd, uint64_t req, uint64_t arg);
int fd_fcntl(int fd, int cmd, uint64_t arg);
int fd_dup(int oldfd);
int fd_dup2(int oldfd, int newfd);
int fd_dup3(int oldfd, int newfd, int flags);
