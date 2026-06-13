#include "common.h"

static bool flag_r, flag_n, flag_u;
static int  key_field = 0; /* 0 = whole line */
static char field_sep = 0; /* 0 = whitespace */

static const char *get_key(const char *line)
{
    if (key_field <= 0) return line;
    int f = 0; const char *p = line;
    while (*p) {
        if (field_sep ? *p == field_sep : isspace((unsigned char)*p)) {
            p++; continue;
        }
        f++;
        if (f == key_field) return p;
        while (*p && (field_sep ? *p != field_sep : !isspace((unsigned char)*p))) p++;
    }
    return p;
}

static int cmp_lines(const void *a, const void *b)
{
    const char *sa = get_key(*(const char *const *)a);
    const char *sb = get_key(*(const char *const *)b);
    int r;
    if (flag_n) {
        long la = strtol(sa, NULL, 10), lb = strtol(sb, NULL, 10);
        r = (la > lb) - (la < lb);
    } else {
        r = strcmp(sa, sb);
    }
    return flag_r ? -r : r;
}

int main(int argc, char **argv)
{
    kx_prog = "sort";
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (strcmp(argv[i], "--") == 0) { i++; break; }
        for (char *o = argv[i]+1; *o; o++) {
            switch (*o) {
            case 'r': flag_r = true; break;
            case 'n': flag_n = true; break;
            case 'u': flag_u = true; break;
            case 'k':
                if (o[1]) { key_field = atoi(o+1); o += strlen(o)-1; }
                else if (i+1 < argc) key_field = atoi(argv[++i]);
                break;
            case 't':
                if (o[1]) { field_sep = o[1]; o += strlen(o)-1; }
                else if (i+1 < argc) field_sep = argv[++i][0];
                break;
            default:
                fprintf(stderr, "%s: invalid option -%c\n", kx_prog, *o); exit(1);
            }
        }
    }
    if (i == argc) argv[argc++] = NULL;
    char **lines = NULL;
    size_t nlines = 0, caplines = 0;
    int rc = 0;
    for (int a = i; a < argc; a++) {
        FILE *f = argv[a] ? fopen(argv[a], "r") : stdin;
        if (!f) { kx_warn(argv[a]); rc = 1; continue; }
        char *line = NULL; size_t cap = 0;
        while (getline(&line, &cap, f) >= 0) {
            if (nlines == caplines) {
                caplines = caplines ? caplines * 2 : 64;
                char **tmp = realloc(lines, caplines * sizeof(*lines));
                if (!tmp) { perror("realloc"); exit(1); }
                lines = tmp;
            }
            lines[nlines++] = strdup(line);
        }
        free(line);
        if (argv[a]) fclose(f);
    }
    qsort(lines, nlines, sizeof(*lines), cmp_lines);
    const char *prev = NULL;
    for (size_t j = 0; j < nlines; j++) {
        if (!flag_u || !prev || strcmp(lines[j], prev) != 0) {
            fputs(lines[j], stdout);
            prev = lines[j];
        }
        if (flag_u && prev != lines[j]) free(lines[j]);
        else if (!flag_u) free(lines[j]);
    }
    if (flag_u && prev) free((void *)prev);
    free(lines);
    return rc;
}
