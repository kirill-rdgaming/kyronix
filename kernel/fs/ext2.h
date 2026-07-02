#pragma once
#include <stdbool.h>
#include <stdint.h>

struct block_device;

bool ext2_mount(struct block_device *dev, const char *mount_point);
bool ext2_check_root(struct block_device *dev);
int ext2_write_file(uint32_t ino_nr, const void *data, uint64_t size);
int ext2_create(const char *path, uint16_t mode, const void *data, uint64_t size);
int ext2_mkdir(const char *path, uint16_t mode);
int ext2_unlink(const char *path);
int ext2_symlink(const char *path, const char *target);
int ext2_sync(void);
