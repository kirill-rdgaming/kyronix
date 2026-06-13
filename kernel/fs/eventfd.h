#pragma once
#include "fs/vfs.h"

int64_t eventfd_read(vfs_file_t* f, char* buf, uint64_t len);
int64_t eventfd_write(vfs_file_t* f, const char* buf, uint64_t len);
int64_t timerfd_read(vfs_file_t* f, char* buf, uint64_t len);
