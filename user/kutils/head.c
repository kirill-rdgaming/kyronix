#include "common.h"

int main(int argc, char **argv)
{
    kx_prog = "head";
    long max = 10;
    int first = 1;
    while (first < argc && argv[first][0] == '-' && argv[first][1]) {
        const char *opt = argv[first]+1;
        if (*opt == 'n') {
            if (opt[1]) { max = strtol(opt+1, NULL, 10); first++; }
            else if (first+1 < argc) { max = strtol(argv[first+1], NULL, 10); first += 2; }
            else kx_die("-n requires argument");
            continue;
        }
        /* -NUM short form */
        char *ep; long v = strtol(opt, &ep, 10);
        if (!*ep && v > 0) { max = v; first++; continue; }
        fprintf(stderr, "%s: invalid option -%s\n", kx_prog, opt); exit(1);
    }
    if (first == argc) argv[argc++] = NULL;
    int rc = 0;
    bool multi = (argc - first) > 1;
    for (int a = first; a < argc; a++) {
        FILE *f = argv[a] ? fopen(argv[a], "r") : stdin;
        if (!f) { kx_warn(argv[a]); rc = 1; continue; }
        if (multi) printf("==> %s <==\n", argv[a]);
        char *line = NULL; size_t cap = 0;
        for (long n = 0; n < max && getline(&line, &cap, f) >= 0; n++)
            fputs(line, stdout);
        free(line);
        if (multi && a+1 < argc) putchar('\n');
        if (argv[a]) fclose(f);
    }
    return rc;
}
