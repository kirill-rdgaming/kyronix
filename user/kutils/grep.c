#include "common.h"

static bool flag_i, flag_n, flag_v, flag_l, flag_c, flag_r;
static const char *pattern;

static char *my_strcasestr(const char *h, const char *n)
{
    if (!*n) return (char *)h;
    size_t nl = strlen(n);
    for (; *h; h++)
        if (strncasecmp(h, n, nl) == 0) return (char *)h;
    return NULL;
}

static int grep_stream(FILE *f, const char *name, bool pname)
{
    char *line = NULL; size_t cap = 0;
    long lineno = 0, count = 0; int rc = 1;
    while (getline(&line, &cap, f) >= 0) {
        lineno++;
        bool hit = flag_i ? my_strcasestr(line, pattern) != NULL
                          : strstr(line, pattern) != NULL;
        if (flag_v) hit = !hit;
        if (!hit) continue;
        rc = 0; count++;
        if (flag_l) { puts(name); free(line); return 0; }
        if (!flag_c) {
            if (pname) printf("%s:", name);
            if (flag_n) printf("%ld:", lineno);
            fputs(line, stdout);
            if (!line[0] || line[strlen(line)-1] != '\n') putchar('\n');
        }
    }
    if (flag_c) {
        if (pname) printf("%s:", name);
        printf("%ld\n", count);
        if (count) rc = 0;
    }
    free(line); return rc;
}

static int grep_path(const char *path, bool pname);

static int grep_dir(const char *path)
{
    DIR *d = opendir(path); if (!d) { kx_warn(path); return 2; }
    int rc = 1; struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char child[PATH_MAX];
        snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
        int r = grep_path(child, true);
        if (r == 0) rc = 0; else if (r == 2 && rc != 0) rc = 2;
    }
    closedir(d); return rc;
}

static int grep_path(const char *path, bool pname)
{
    struct stat st;
    if (stat(path, &st) < 0) { kx_warn(path); return 2; }
    if (S_ISDIR(st.st_mode)) {
        if (flag_r) return grep_dir(path);
        fprintf(stderr, "%s: %s: Is a directory\n", kx_prog, path); return 2;
    }
    FILE *f = fopen(path, "r"); if (!f) { kx_warn(path); return 2; }
    int rc = grep_stream(f, path, pname); fclose(f); return rc;
}

int main(int argc, char **argv)
{
    kx_prog = "grep";
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (strcmp(argv[i], "--") == 0) { i++; break; }
        for (char *o = argv[i]+1; *o; o++) {
            switch (*o) {
            case 'i': flag_i = true; break;
            case 'n': flag_n = true; break;
            case 'v': flag_v = true; break;
            case 'l': flag_l = true; break;
            case 'c': flag_c = true; break;
            case 'r': case 'R': flag_r = true; break;
            case 'e':
                if (o[1]) { pattern = o+1; o += strlen(o)-1; }
                else if (i+1 < argc) pattern = argv[++i];
                else kx_die("-e requires argument");
                break;
            default:
                fprintf(stderr, "%s: invalid option -%c\n", kx_prog, *o); exit(2);
            }
        }
    }
    if (!pattern) {
        if (i >= argc) kx_die("usage: grep [OPTIONS] PATTERN [FILE...]");
        pattern = argv[i++];
    }
    bool pname = (argc - i) > 1 || flag_r;
    int rc = 1;
    if (i == argc) {
        rc = grep_stream(stdin, "(stdin)", false);
    } else {
        for (; i < argc; i++) {
            int r = grep_path(argv[i], pname);
            if (r == 0) rc = 0; else if (r == 2 && rc == 1) rc = 2;
        }
    }
    return rc;
}
