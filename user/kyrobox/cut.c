#include "common.h"

#define MAX_FIELDS 256

static bool flag_c, flag_f;
static char  delim = '\t';
static bool  fields[MAX_FIELDS+1]; /* 1-indexed */
static long  range_lo, range_hi;   /* used only with -c */

static void parse_field_list(const char *spec)
{
    /* Accepts: N, N-M, N-, -M, comma-separated */
    char *buf = strdup(spec); char *tok = strtok(buf, ",");
    while (tok) {
        char *dash = strchr(tok, '-');
        if (!dash) {
            long v = strtol(tok, NULL, 10);
            if (v >= 1 && v <= MAX_FIELDS) fields[v] = true;
        } else {
            long lo = (dash == tok) ? 1 : strtol(tok, NULL, 10);
            long hi = dash[1] ? strtol(dash+1, NULL, 10) : MAX_FIELDS;
            if (lo < 1) lo = 1;
            if (hi > MAX_FIELDS) hi = MAX_FIELDS;
            for (long v = lo; v <= hi; v++) fields[v] = true;
        }
        tok = strtok(NULL, ",");
    }
    free(buf);
}

static void process_char(const char *line, long lo, long hi)
{
    long col = 1;
    for (const char *p = line; *p && *p != '\n'; p++, col++)
        if (col >= lo && col <= hi) putchar(*p);
    putchar('\n');
}

static void process_field(const char *line)
{
    char *buf = strdup(line); int n = 0; bool printed = false;
    /* strip trailing newline */
    size_t l = strlen(buf); if (l && buf[l-1] == '\n') buf[l-1] = 0;
    char *p = buf;
    while (true) {
        n++;
        char *end = strchr(p, delim);
        if (end) *end = 0;
        if (n <= MAX_FIELDS && fields[n]) {
            if (printed) putchar(delim);
            fputs(p, stdout); printed = true;
        }
        if (!end) break;
        p = end + 1;
    }
    putchar('\n'); free(buf);
}

static void run(FILE *f)
{
    char *line = NULL; size_t cap = 0;
    while (getline(&line, &cap, f) >= 0) {
        if (flag_c) process_char(line, range_lo, range_hi);
        else process_field(line);
    }
    free(line);
}

int main(int argc, char **argv)
{
    kx_prog = "cut";
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (strcmp(argv[i], "--") == 0) { i++; break; }
        for (char *o = argv[i]+1; *o; o++) {
            switch (*o) {
            case 'c':
                flag_c = true;
                if (o[1]) {
                    const char *spec = o+1;
                    char *d = strchr(spec, '-');
                    range_lo = strtol(spec, NULL, 10);
                    if (!range_lo) range_lo = 1;
                    range_hi = d && d[1] ? strtol(d+1, NULL, 10) : (d ? LONG_MAX : range_lo);
                    o += strlen(o)-1;
                } else if (i+1 < argc) {
                    const char *spec = argv[++i];
                    char *d = strchr(spec, '-');
                    range_lo = strtol(spec, NULL, 10);
                    if (!range_lo) range_lo = 1;
                    range_hi = d && d[1] ? strtol(d+1, NULL, 10) : (d ? LONG_MAX : range_lo);
                } else kx_die("-c requires argument");
                break;
            case 'f':
                flag_f = true;
                if (o[1]) { parse_field_list(o+1); o += strlen(o)-1; }
                else if (i+1 < argc) parse_field_list(argv[++i]);
                else kx_die("-f requires argument");
                break;
            case 'd':
                if (o[1]) { delim = o[1]; o += strlen(o)-1; }
                else if (i+1 < argc) delim = argv[++i][0];
                else kx_die("-d requires argument");
                break;
            default:
                fprintf(stderr, "%s: invalid option -%c\n", kx_prog, *o); exit(1);
            }
        }
    }
    if (!flag_c && !flag_f) kx_die("usage: cut -c N[-M] [FILE...]\n       cut -f LIST [-d DELIM] [FILE...]");
    if (i == argc) argv[argc++] = NULL;
    int rc = 0;
    for (int a = i; a < argc; a++) {
        FILE *f = argv[a] ? fopen(argv[a], "r") : stdin;
        if (!f) { kx_warn(argv[a]); rc = 1; continue; }
        run(f);
        if (argv[a]) fclose(f);
    }
    return rc;
}
