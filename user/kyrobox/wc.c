#include "common.h"

int main(int argc, char **argv)
{
    kx_prog = "wc";
    bool do_l = false, do_w = false, do_c = false;
    int first = 1;
    while (first < argc && argv[first][0] == '-' && argv[first][1]) {
        if (strcmp(argv[first], "--") == 0) { first++; break; }
        for (char *p = argv[first]+1; *p; p++) {
            switch (*p) {
            case 'l': do_l = true; break;
            case 'w': do_w = true; break;
            case 'c': do_c = true; break;
            default: fprintf(stderr, "%s: invalid option -%c\n", kx_prog, *p); exit(1);
            }
        }
        first++;
    }
    if (!do_l && !do_w && !do_c) do_l = do_w = do_c = true;
    if (first == argc) argv[argc++] = NULL;
    int rc = 0;
    for (int a = first; a < argc; a++) {
        FILE *f = argv[a] ? fopen(argv[a], "r") : stdin;
        if (!f) { kx_warn(argv[a]); rc = 1; continue; }
        long lines = 0, words = 0, bytes = 0;
        int inword = 0, c;
        while ((c = fgetc(f)) != EOF) {
            bytes++;
            if (c == '\n') lines++;
            if (isspace(c)) inword = 0;
            else if (!inword) { words++; inword = 1; }
        }
        bool first_col = true;
#define EMIT(cond, val) if (cond) { if (!first_col) putchar(' '); printf("%ld", val); first_col = false; }
        EMIT(do_l, lines);
        EMIT(do_w, words);
        EMIT(do_c, bytes);
#undef EMIT
        if (argv[a]) printf(" %s", argv[a]);
        putchar('\n');
        if (argv[a]) fclose(f);
    }
    return rc;
}
