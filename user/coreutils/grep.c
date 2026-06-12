#include "common.h"
int main(int argc, char **argv)
{
    kx_prog = "grep";
    if (argc < 2) kx_die("usage: grep PATTERN [FILE...]");
    const char *pat = argv[1];
    int first = 2;
    if (first == argc) argv[argc++] = NULL;
    int matched = 0, rc = 1;
    for (int a = first; a < argc; a++) {
        FILE *f = argv[a] ? fopen(argv[a], "r") : stdin;
        if (!f) {
            kx_warn(argv[a]);
            rc = 2;
            continue;
        }
        char *line = NULL;
        size_t cap = 0;
        while (getline(&line, &cap, f) >= 0) {
            if (strstr(line, pat)) {
                fputs(line, stdout);
                matched = 1;
            }
        }
        free(line);
        if (argv[a]) fclose(f);
    }
    return matched ? 0 : rc;
}
