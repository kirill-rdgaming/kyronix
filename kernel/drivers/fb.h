#pragma once

#include <stdint.h>
#include <stddef.h>
#include "../boot/limine.h"

#define RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))

#define COLOR_BLACK   RGB(  0,   0,   0)
#define COLOR_WHITE   RGB(255, 255, 255)
#define COLOR_RED     RGB(220,  50,  47)
#define COLOR_GREEN   RGB( 42, 161, 152)
#define COLOR_BLUE    RGB( 38, 139, 210)
#define COLOR_YELLOW  RGB(181, 137,   0)
#define COLOR_CYAN    RGB(  0, 210, 210)
#define COLOR_GRAY    RGB( 88,  88, 100)
#define COLOR_BG      RGB( 10,  10,  20)

typedef struct {
    void    *addr;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    // for cursor
    uint32_t col, row;
    uint32_t fg, bg;
} fb_t;

extern fb_t g_fb;

void fb_init(struct limine_framebuffer *fb);
void fb_clear(uint32_t color);
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

void fb_putchar(char c);
void fb_write(const char *s);
void fb_set_color(uint32_t fg, uint32_t bg);
