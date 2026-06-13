#include "common.h"

#define MAX_EXPRS 32
typedef struct { char old[4096]; char new[4096]; bool global; bool del; bool print; } expr_t;

static expr_t exprs[MAX_EXPRS];
static int nexpr;
static bool flag_n; /* suppress default print */

static void add_expr(const char *s)
{
    if (nexpr >= MAX_EXPRS) kx_die("too many -e expressions");
    expr_t *e = &exprs[nexpr++];
    memset(e, 0, sizeof(*e));
    if (strcmp(s, "d") == 0) { e->del = true; return; }
    if (strcmp(s, "p") == 0) { e->print = true; return; }
    if (s[0] != 's') kx_die("only s/OLD/NEW/[g], d, p commands supported");
    char buf[4096]; snprintf(buf, sizeof(buf), "%s", s+2);
    char sep = s[1]; char sep_str[2] = { sep, 0 };
    char *old_s = buf;
    char *slash = strchr(old_s, sep);
    if (!slash) kx_die("bad s expression");
    *slash++ = 0;
    char *new_s = slash;
    slash = strchr(new_s, sep);
    if (!slash) kx_die("bad s expression");
    *slash++ = 0;
    snprintf(e->old, sizeof(e->old), "%s", old_s);
    snprintf(e->new, sizeof(e->new), "%s", new_s);
    e->global = strchr(slash, 'g') != NULL;
    (void)sep_str;
}

static bool run_exprs(char *line, size_t *cap)
{
    bool print_it = !flag_n;
    for (int i = 0; i < nexpr; i++) {
        expr_t *e = &exprs[i];
        if (e->del) { return false; }
        if (e->print) { fputs(line, stdout); continue; }
        /* s command */
        size_t oldlen = strlen(e->old); if (!oldlen) continue;
        char out[65536]; char *dst = out; char *p = line;
        size_t rem = sizeof(out)-1;
        while (*p) {
            char *m = strstr(p, e->old);
            if (!m) { size_t l = strlen(p); if (l > rem) l = rem; memcpy(dst, p, l); dst += l; rem -= l; break; }
            size_t pre = (size_t)(m - p); if (pre > rem) pre = rem;
            memcpy(dst, p, pre); dst += pre; rem -= pre;
            size_t nl = strlen(e->new); if (nl > rem) nl = rem;
            memcpy(dst, e->new, nl); dst += nl; rem -= nl;
            p = m + oldlen;
            if (!e->global) { size_t l = strlen(p); if (l > rem) l = rem; memcpy(dst, p, l); dst += l; rem -= l; break; }
        }
        *dst = 0;
        /* grow line buffer if needed */
        size_t olen = (size_t)(dst - out);
        if (olen+1 > *cap) { *cap = olen+64; line = realloc(line, *cap); if (!line) exit(1); }
        memcpy(line, out, olen+1);
    }
    if (print_it) fputs(line, stdout);
    return false; /* already printed */
}

static void run_stream(FILE *f)
{
    char *line = NULL; size_t cap = 0;
    while (getline(&line, &cap, f) >= 0) {
        bool skip = run_exprs(line, &cap);
        (void)skip;
    }
    free(line);
}

int main(int argc, char **argv)
{
    kx_prog = "sed";
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (strcmp(argv[i], "--") == 0) { i++; break; }
        for (char *o = argv[i]+1; *o; o++) {
            switch (*o) {
            case 'n': flag_n = true; break;
            case 'e':
                if (o[1]) { add_expr(o+1); o += strlen(o)-1; }
                else if (i+1 < argc) add_expr(argv[++i]);
                else kx_die("-e requires argument");
                break;
            default:
                fprintf(stderr, "%s: invalid option -%c\n", kx_prog, *o); exit(1);
            }
        }
    }
    if (nexpr == 0) {
        if (i >= argc) kx_die("usage: sed [-n] [-e EXPR] ... [FILE...]");
        add_expr(argv[i++]);
    }
    if (i == argc) argv[argc++] = NULL;
    int rc = 0;
    for (int a = i; a < argc; a++) {
        FILE *f = argv[a] ? fopen(argv[a], "r") : stdin;
        if (!f) { kx_warn(argv[a]); rc = 1; continue; }
        run_stream(f);
        if (argv[a]) fclose(f);
    }
    return rc;
}
