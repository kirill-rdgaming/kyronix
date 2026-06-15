#include "common.h"

int main(int argc, char **argv)
{
    kx_prog = "tail";
    long max = 10;
    bool flag_f = false;
    int first = 1;
    while (first < argc && argv[first][0] == '-' && argv[first][1]) {
        const char *opt = argv[first]+1;
        if (*opt == 'f') { flag_f = true; first++; continue; }
        if (*opt == 'n') {
            if (opt[1]) { max = strtol(opt+1, NULL, 10); first++; }
            else if (first+1 < argc) { max = strtol(argv[first+1], NULL, 10); first += 2; }
            else kx_die("-n requires argument");
            continue;
        }
        /* -NUM short form: -5 means -n 5 */
        char *ep; long v = strtol(opt, &ep, 10);
        if (!*ep && v > 0) { max = v; first++; continue; }
        fprintf(stderr, "%s: invalid option -%s\n", kx_prog, opt); exit(1);
    }
    if (max < 0) max = 0;
    if (first == argc) argv[argc++] = NULL;
    int rc = 0;
    for (int a = first; a < argc; a++) {
        FILE *f = argv[a] ? fopen(argv[a], "r") : stdin;
        if (!f) { kx_warn(argv[a]); rc = 1; continue; }
        if (max == 0) { if (argv[a]) fclose(f); continue; }
        /* ring buffer of last `max` lines */
        char **ring = calloc((size_t)max, sizeof(char *));
        if (!ring) { perror("calloc"); exit(1); }
        char *line = NULL; size_t cap = 0;
        long n = 0;
        while (getline(&line, &cap, f) >= 0) {
            free(ring[n % max]);
            ring[n % max] = strdup(line);
            n++;
        }
        long start = n > max ? n - max : 0;
        for (long i = start; i < n; i++) { fputs(ring[i % max], stdout); }
        for (long i = 0; i < max; i++) free(ring[i]);
        free(ring); free(line);

        if (flag_f && argv[a]) {
            /* follow: keep reading new data appended to file */
            for (;;) {
                if (getline(&line, &cap, f) >= 0) { fputs(line, stdout); fflush(stdout); }
                else { clearerr(f); sleep(1); }
            }
        }
        if (argv[a]) fclose(f);
    }
    return rc;
}
