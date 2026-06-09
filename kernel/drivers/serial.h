#pragma once

#include <stdint.h>
#include <stdbool.h>

#define COM1 0x3F8
#define COM2 0x2F8

bool serial_init(uint16_t port);
void serial_putchar(uint16_t port, char c);
void serial_write(uint16_t port, const char *s);
