#pragma once
#include <stdbool.h>
#include <stdint.h>

int64_t tty_read(char* buf, uint64_t len);
int64_t tty_write(const char* buf, uint64_t len);
bool tty_data_ready(void);
void tty_putchar(char c);
int tty_get_fg_pgid(void);
void tty_set_fg_pgid(int pgid);
uint32_t tty_get_lflag(void);
void tty_set_lflag(uint32_t lflag);
void tty_check_signals(void);
