#include "common.h"

int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"((long) 169), "D"((long) 0) : "rcx", "r11", "memory");
    if (ret < 0) {
        fprintf(stderr, "reboot: %s\n", strerror((int) -ret));
        return 1;
    }
    return 0;
}
