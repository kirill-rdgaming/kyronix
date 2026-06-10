#include "tty.h"
#include "../arch/x86_64/cpu.h"
#include "../proc/proc.h"
#include "../proc/signal.h"
#include "fb.h"
#include "kbd.h"
#include "serial.h"

#define ISIG 0000001
#define EINTR 4
#define TTY_BUF_SIZE 256

static uint8_t tty_buf[TTY_BUF_SIZE];
static volatile int tty_buf_head;
static volatile int tty_buf_tail;
static int tty_fg_pgid = 1;
static uint32_t tty_lflag = 0x8A3B;

static bool tty_buf_empty(void)
{
    return tty_buf_head == tty_buf_tail;
}

static bool tty_buf_full(void)
{
    return ((tty_buf_head + 1) % TTY_BUF_SIZE) == tty_buf_tail;
}

static void tty_enqueue(uint8_t c)
{
    int next = (tty_buf_head + 1) % TTY_BUF_SIZE;
    if (next != tty_buf_tail)
    {
        tty_buf[tty_buf_head] = c;
        tty_buf_head = next;
    }
}

static int tty_dequeue(void)
{
    if (tty_buf_empty())
        return -1;
    uint8_t c = tty_buf[tty_buf_tail];
    tty_buf_tail = (tty_buf_tail + 1) % TTY_BUF_SIZE;
    return (int) c;
}

static void tty_send_sig_pgid(int sig)
{
    for (int i = 0; i < PROC_MAX; i++)
    {
        if (g_proctable[i].state == PROC_UNUSED)
            continue;
        if (g_proctable[i].pgid == tty_fg_pgid)
            proc_send_signal(&g_proctable[i], sig);
    }
}

static void tty_process_input(void)
{
    if (serial_data_ready(COM1))
    {
        uint8_t c = serial_getchar(COM1);
        if ((tty_lflag & ISIG) && (c == 0x03 || c == 0x1C))
        {
            tty_putchar('^');
            tty_putchar((char) ('@' + c));
            tty_putchar('\n');
            tty_send_sig_pgid((c == 0x03) ? SIGINT : SIGQUIT);
        }
        else
        {
            if (c == '\r')
                c = '\n';
            if (!tty_buf_full())
                tty_enqueue(c);
        }
    }

    if (kbd_data_ready())
    {
        int c = kbd_getchar();
        if (c > 0)
        {
            if ((tty_lflag & ISIG) && (c == 0x03 || c == 0x1C))
            {
                tty_putchar('^');
                tty_putchar((char) ('@' + c));
                tty_putchar('\n');
                tty_send_sig_pgid((c == 0x03) ? SIGINT : SIGQUIT);
            }
            else
            {
                if (c == '\r')
                    c = '\n';
                if (!tty_buf_full())
                    tty_enqueue((uint8_t) c);
            }
        }
    }
}

int64_t tty_read(char* buf, uint64_t len)
{
    if (!len)
        return 0;

    uint64_t i = 0;
    for (;;)
    {
        if (i >= len)
            break;

        tty_process_input();

        int c = tty_dequeue();
        if (c >= 0)
        {
            if (c == 0x04)
            {
                if (i == 0)
                    return 0;
                break;
            }
            buf[i++] = (char) c;
            continue;
        }

        if (i > 0)
            break;

        if (g_current_proc && (g_current_proc->pending_sigs & ~g_current_proc->sig_mask))
            return -(int64_t) EINTR;

        sched_yield_blocking();
        cpu_relax();
    }
    return (int64_t) i;
}

int64_t tty_write(const char* buf, uint64_t len)
{
    tty_process_input();
    for (uint64_t i = 0; i < len; i++)
    {
        serial_putchar(COM1, buf[i]);
        if (g_fb.addr)
            fb_putchar(buf[i]);
    }
    return (int64_t) len;
}

bool tty_data_ready(void)
{
    return !tty_buf_empty() || serial_data_ready(COM1) || kbd_data_ready();
}

void tty_putchar(char c)
{
    serial_putchar(COM1, c);
    if (g_fb.addr)
        fb_putchar(c);
}

void tty_check_signals(void)
{
    tty_process_input();
}

int tty_get_fg_pgid(void)
{
    return tty_fg_pgid;
}

void tty_set_fg_pgid(int pgid)
{
    tty_fg_pgid = pgid;
}

uint32_t tty_get_lflag(void)
{
    return tty_lflag;
}

void tty_set_lflag(uint32_t lflag)
{
    tty_lflag = lflag;
}
