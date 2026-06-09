#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "boot/limine.h"
#include "arch/x86_64/cpu.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/syscall_setup.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "drivers/serial.h"
#include "drivers/fb.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "exec/process.h"
#include "fs/vfs.h"
#include "fs/cpio.h"
#include "proc/proc.h"

LIMINE_REQUESTS_START_MARKER;
LIMINE_BASE_REVISION(3);

static volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = NULL,
};

static volatile struct limine_memmap_request mmap_req = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = NULL,
};

static volatile struct limine_hhdm_request hhdm_req = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL,
};

static volatile struct limine_module_request mod_req = {
    .id       = LIMINE_MODULE_REQUEST,
    .revision = 0,
    .response = NULL,
    .internal_module_count = 0,
    .internal_modules      = NULL,
};

LIMINE_REQUESTS_END_MARKER;

static void kernel_putchar(char c, void *ctx) {
    (void)ctx;
    serial_putchar(COM1, c);
    if (g_fb.addr) fb_putchar(c);
}

static const char *memmap_type_name(uint64_t type) {
    switch (type) {
        case LIMINE_MEMMAP_USABLE:                 return "Usable";
        case LIMINE_MEMMAP_RESERVED:               return "Reserved";
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:       return "ACPI reclaimable";
        case LIMINE_MEMMAP_ACPI_NVS:               return "ACPI NVS";
        case LIMINE_MEMMAP_BAD_MEMORY:             return "Bad memory";
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return "Bootloader reclaimable";
        case LIMINE_MEMMAP_KERNEL_AND_MODULES:     return "Kernel & modules";
        case LIMINE_MEMMAP_FRAMEBUFFER:            return "Framebuffer";
        default:                                   return "Unknown";
    }
}

static void print_banner(void) {
    fb_set_color(COLOR_CYAN, COLOR_BG);
    kprintf("   K  K  Y   Y  RRRR    OOO   N   N  III  X   X \n");
    kprintf("   K K    Y Y   R   R  O   O  NN  N   I    X X  \n");
    kprintf("   KK      Y    RRRR   O   O  N N N   I     X   \n");
    kprintf("   K K     Y    R R    O   O  N  NN   I    X X  \n");
    kprintf("   K  K    Y    R  RR   OOO   N   N  III  X   X \n");
    kprintf("===================================================\n");
    kprintf("                                           \n");

    fb_set_color(COLOR_WHITE, COLOR_BG);
}

static void print_memmap(void) {
    if (!mmap_req.response) {
        kprintf("[warn] no memory map\n");
        return;
    }
    struct limine_memmap_response *resp = mmap_req.response;
    uint64_t total_usable = 0;

    fb_set_color(COLOR_YELLOW, COLOR_BG);
    kprintf("Memory map (%lu entries):\n", resp->entry_count);
    fb_set_color(COLOR_WHITE, COLOR_BG);

    for (uint64_t i = 0; i < resp->entry_count; i++) {
        struct limine_memmap_entry *e = resp->entries[i];
        kprintf("  %016lx-%016lx  %s\n",
                e->base, e->base + e->length,
                memmap_type_name(e->type));
        if (e->type == LIMINE_MEMMAP_USABLE)
            total_usable += e->length;
    }
    kprintf("  Usable: %lu MiB\n\n", total_usable / (1024 * 1024));
}

void kmain(void) {
    serial_init(COM1);
    printf_set_putchar(kernel_putchar, NULL);

    gdt_init();
    idt_init();

    if (!mmap_req.response || !hhdm_req.response) {
        kprintf("FATAL: no memory map or HHDM from bootloader\n");
        cpu_halt();
    }
    pmm_init(mmap_req.response, hhdm_req.response->offset);
    vmm_init();
    heap_init();
    syscall_init();
    proc_init();
    vfs_init();

    if (!fb_req.response || fb_req.response->framebuffer_count < 1)
        cpu_halt();

    struct limine_framebuffer *lfb = fb_req.response->framebuffers[0];
    fb_init(lfb);
    fb_clear(COLOR_BG);

    print_banner();

    {
        void *p[4];
        bool pmm_ok = true;
        for (int i = 0; i < 4; i++) {
            p[i] = pmm_alloc();
            if (!p[i]) { pmm_ok = false; break; }
            volatile uint64_t *v = phys_to_virt((uint64_t)p[i]);
            *v = 0xDEADBEEFCAFEBABEULL;
            if (*v != 0xDEADBEEFCAFEBABEULL) { pmm_ok = false; break; }
        }
        for (int i = 0; i < 4 && pmm_ok; i++)
            for (int j = i + 1; j < 4; j++)
                if (p[i] == p[j]) { pmm_ok = false; break; }

        for (int i = 0; i < 4; i++) if (p[i]) pmm_free(p[i]);

        fb_set_color(pmm_ok ? COLOR_GREEN : COLOR_RED, COLOR_BG);
        kprintf("pmm test : %s\n", pmm_ok ? "PASS" : "FAIL");
        fb_set_color(COLOR_WHITE, COLOR_BG);
    }

    {
        const uint64_t test_virt = 0xffff900000001000ULL;
        uint64_t test_phys = (uint64_t)pmm_alloc();
        bool vmm_ok = false;

        if (test_phys) {
            int r = vmm_map(&g_kernel_space, test_virt, test_phys, VMM_KDATA);
            if (r == 0) {
                volatile uint64_t *p = (volatile uint64_t *)test_virt;
                *p = 0xC0FFEE00DEADC0DEULL;
                vmm_ok = (*p == 0xC0FFEE00DEADC0DEULL);
                vmm_unmap(&g_kernel_space, test_virt);
            }
            pmm_free((void *)test_phys);
        }

        fb_set_color(vmm_ok ? COLOR_GREEN : COLOR_RED, COLOR_BG);
        kprintf("vmm test : %s\n", vmm_ok ? "PASS" : "FAIL");
        fb_set_color(COLOR_WHITE, COLOR_BG);
    }

    {
        bool heap_ok = true;

        uint8_t *a = kmalloc(64);
        uint8_t *b = kmalloc(128);
        uint8_t *c = kmalloc(256);

        heap_ok = a && b && c && (a != b) && (b != c) && (a != c);

        if (heap_ok) {
            memset(a, 0xAA, 64);
            memset(b, 0xBB, 128);
            memset(c, 0xCC, 256);
            for (int i = 0; i < 64  && heap_ok; i++) if (a[i] != 0xAA) heap_ok = false;
            for (int i = 0; i < 128 && heap_ok; i++) if (b[i] != 0xBB) heap_ok = false;
            for (int i = 0; i < 256 && heap_ok; i++) if (c[i] != 0xCC) heap_ok = false;
        }

        kfree(b);
        uint8_t *d = kmalloc(64);
        heap_ok = heap_ok && (d != NULL);
        if (d) memset(d, 0xDD, 64);

        uint8_t *a2 = krealloc(a, 512);
        heap_ok = heap_ok && (a2 != NULL);
        if (a2) {
            for (int i = 0; i < 64 && heap_ok; i++)
                if (a2[i] != 0xAA) heap_ok = false;
            kfree(a2);
        } else {
            kfree(a);
        }
        kfree(c);
        kfree(d);

        fb_set_color(heap_ok ? COLOR_GREEN : COLOR_RED, COLOR_BG);
        kprintf("heap test: %s\n", heap_ok ? "PASS" : "FAIL");
        fb_set_color(COLOR_WHITE, COLOR_BG);
        heap_stats();
    }
    
    if (hhdm_req.response)
        kprintf("hhdm offset : 0x%016lx\n", hhdm_req.response->offset);

    kprintf("fb : %lux%lu  %u bpp\n\n",
            lfb->width, lfb->height, (unsigned)lfb->bpp);

    print_memmap();

    if (mod_req.response && mod_req.response->module_count > 0) {
        struct limine_file *initrd = mod_req.response->modules[0];
        kprintf("initrd: %s  (%lu bytes)\n", initrd->path, initrd->size);
        if (cpio_load(initrd->address, initrd->size) < 0)
            kprintf("[warn] initrd parse failed\n");
    } else {
        kprintf("[warn] no initrd module\n");
    }

    {
        vfs_node_t *init_node = vfs_lookup("/init");
        if (!init_node)
            init_node = vfs_lookup("/sbin/init");
        if (!init_node)
            init_node = vfs_lookup("/bin/init");

        if (init_node && init_node->type == VFS_TYPE_REG && init_node->data) {
            kprintf("Launching /init  (%lu bytes)\n", init_node->size);
            fb_set_color(COLOR_CYAN, COLOR_BG);
            kprintf("Entering ring 3...\n");
            fb_set_color(COLOR_WHITE, COLOR_BG);
            if (process_exec(init_node->data, init_node->size, "/init") < 0)
                kprintf("FATAL: process_exec failed\n");
        } else {
            kprintf("FATAL: /init not found in initrd\n");
        }
    }

    fb_set_color(COLOR_GRAY, COLOR_BG);
    kprintf("Kernel halted.\n");
    cpu_halt();
}
