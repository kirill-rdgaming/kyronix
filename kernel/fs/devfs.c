#include "devfs.h"
#include "fs/pipe.h"
#include "fs/vfs.h"
#include "drivers/tty.h"
#include "lib/string.h"
#include "mm/vmm.h"
#include "proc/proc.h"
#include "syscall/syscall.h"

#define EFAULT 14
#define EINVAL 22
#define EPERM   1

static int64_t devmem_mmap(vfs_node_t* n, uint64_t off, uint64_t len,
                           uint64_t va, uint64_t vflags)
{
    (void)n;
    proc_t* p = g_current_proc;
    if (!p || !p->space) return -EINVAL;
    if (p->euid != 0) return -(int64_t)EPERM;
    off &= ~0xFFFULL;
    uint64_t flags = vflags | VMM_PRESENT | VMM_USER | VMM_WRITE;
    for (uint64_t o = 0; o < len; o += 0x1000)
        vmm_map(p->space, va + o, off + o, flags);
    return (int64_t)va;
}

static int64_t dev_null_read(vfs_node_t* n, char* buf, uint64_t len, uint64_t off)
{
    (void)n;
    (void)buf;
    (void)len;
    (void)off;
    return 0;
}

static int64_t dev_null_write(vfs_node_t* n, const char* buf, uint64_t len)
{
    (void)n;
    (void)buf;
    return (int64_t)len;
}

static int64_t dev_zero_read(vfs_node_t* n, char* buf, uint64_t len, uint64_t off)
{
    (void)n;
    (void)off;
    memset(buf, 0, len);
    return (int64_t)len;
}

static int64_t dev_urandom_read(vfs_node_t* n, char* buf, uint64_t len, uint64_t off)
{
    (void)n;
    (void)off;
    static uint64_t s = 0xdeadbeef13579aceULL;
    uint8_t* p = (uint8_t*)buf;
    for (uint64_t i = 0; i < len; i++) {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        p[i] = (uint8_t)s;
    }
    return (int64_t)len;
}

static int64_t dev_tty_read(vfs_node_t* n, char* buf, uint64_t len, uint64_t off)
{
    (void)n;
    (void)off;
    return tty_read(buf, len);
}

static int64_t dev_tty_write(vfs_node_t* n, const char* buf, uint64_t len)
{
    (void)n;
    return tty_write(buf, len);
}

static pipe_t* g_pty_m2s;
static pipe_t* g_pty_s2m;

static int64_t pty_master_read(vfs_node_t* n, char* buf, uint64_t len, uint64_t off)
{
    (void)n;
    (void)off;
    return g_pty_s2m ? pipe_read(g_pty_s2m, buf, len) : -(int64_t)EINVAL;
}

static int64_t pty_master_write(vfs_node_t* n, const char* buf, uint64_t len)
{
    (void)n;
    return g_pty_m2s ? pipe_write(g_pty_m2s, buf, len) : -(int64_t)EINVAL;
}

static int64_t pty_slave_read(vfs_node_t* n, char* buf, uint64_t len, uint64_t off)
{
    (void)n;
    (void)off;
    return g_pty_m2s ? pipe_read(g_pty_m2s, buf, len) : -(int64_t)EINVAL;
}

static int64_t pty_slave_write(vfs_node_t* n, const char* buf, uint64_t len)
{
    (void)n;
    if (!g_pty_s2m) return -(int64_t)EINVAL;
    int64_t done = 0;
    for (uint64_t i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            static const char cr = '\r';
            pipe_write(g_pty_s2m, &cr, 1);
        }
        pipe_write(g_pty_s2m, buf + i, 1);
        done++;
    }
    return done;
}

static bool pty_master_pollin(vfs_node_t* n)
{
    (void)n;
    return g_pty_s2m && g_pty_s2m->count > 0;
}

static bool pty_slave_pollin(vfs_node_t* n)
{
    (void)n;
    return g_pty_m2s && g_pty_m2s->count > 0;
}

static int64_t pty_master_ioctl(vfs_node_t* n, uint64_t req, uint64_t arg)
{
    (void)n;
    switch (req) {
    case 0x80045430: /* TIOCGPTN */
        if (arg && !uptr_ok_w((void*)(uintptr_t)arg, sizeof(int))) return -EFAULT;
        if (arg) *(int*)(uintptr_t)arg = 0;
        return 0;
    case 0x40045431: /* TIOCSPTLCK */
    case 0x80045439: /* TIOCGPTLCK */
    case 0x80045432: /* TIOCGDEV */
        if (arg && req != 0x40045431 && !uptr_ok_w((void*)(uintptr_t)arg, sizeof(int)))
            return -EFAULT;
        if (arg && req != 0x40045431)
            *(int*)(uintptr_t)arg = 0;
        return 0;
    }
    return 0;
}

void devfs_init(void)
{
    vfs_mkdir_p("/dev", 0755);
    vfs_mkdir_p("/dev/input", 0755);

    vfs_create_chr("/dev/null",    dev_null_read,    dev_null_write);
    vfs_create_chr("/dev/zero",    dev_zero_read,    dev_null_write);
    vfs_create_chr("/dev/urandom", dev_urandom_read, dev_null_write);
    vfs_create_chr("/dev/random",  dev_urandom_read, dev_null_write);
    {
        vfs_node_t* mn = vfs_create_chr("/dev/mem", dev_null_read, dev_null_write);
        if (mn) {
            mn->mode = S_IFCHR | 0600;
            mn->chr_mmap = devmem_mmap;
        }
    }

    vfs_mkdir_p("/dev/pts", 0755);
    g_pty_m2s = pipe_alloc();
    g_pty_s2m = pipe_alloc();
    if (g_pty_m2s && g_pty_s2m) {
        g_pty_m2s->read_refs = g_pty_m2s->write_refs = 1;
        g_pty_s2m->read_refs = g_pty_s2m->write_refs = 1;
        vfs_node_t* ptmx = vfs_create_chr("/dev/ptmx", pty_master_read, pty_master_write);
        if (ptmx) {
            ptmx->chr_ioctl = pty_master_ioctl;
            ptmx->chr_pollin = pty_master_pollin;
        }
        vfs_node_t* pts0 = vfs_create_chr("/dev/pts/0", pty_slave_read, pty_slave_write);
        if (pts0)
            pts0->chr_pollin = pty_slave_pollin;
    }

    vfs_create_chr("/dev/tty", dev_tty_read, dev_tty_write);
    vfs_create_chr("/dev/stdin", dev_tty_read, dev_null_write);
    vfs_create_chr("/dev/stdout", dev_null_read, dev_tty_write);
    vfs_create_chr("/dev/stderr", dev_null_read, dev_tty_write);
    vfs_create_symlink("/dev/console", "/dev/tty");
    vfs_create_symlink("/dev/fd", "/proc/self/fd");
}
