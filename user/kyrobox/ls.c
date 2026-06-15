#include "common.h"
#include <grp.h>

static bool flag_l, flag_a, flag_R, flag_h, flag_1;

static char ftype(mode_t m)
{
    if (S_ISDIR(m))  return 'd';
    if (S_ISLNK(m))  return 'l';
    if (S_ISCHR(m))  return 'c';
    if (S_ISBLK(m))  return 'b';
    if (S_ISFIFO(m)) return 'p';
    if (S_ISSOCK(m)) return 's';
    return '-';
}

static void mode_string(mode_t m, char out[11])
{
    out[0] = ftype(m);
    const char *c = "rwx";
    for (int i = 0; i < 9; i++) out[i+1] = (m & (1 << (8-i))) ? c[i%3] : '-';
    out[10] = 0;
}

static void human_size(long long sz, char *buf, size_t bsz)
{
    const char *sfx[] = { "B","K","M","G","T" };
    int i = 0; double v = (double)sz;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; i++; }
    if (i == 0) snprintf(buf, bsz, "%lld", sz);
    else snprintf(buf, bsz, "%.1f%s", v, sfx[i]);
}

static int list_dir(const char *path, bool head);

static void print_entry(const char *dirpath, const char *name)
{
    char full[PATH_MAX];
    snprintf(full, sizeof(full), "%s/%s", dirpath, name);
    struct stat st;
    if (lstat(full, &st) < 0) { kx_warn(full); return; }
    if (flag_l) {
        char m[11]; mode_string(st.st_mode, m);
        char szbuf[32];
        if (flag_h) human_size((long long)st.st_size, szbuf, sizeof(szbuf));
        else snprintf(szbuf, sizeof(szbuf), "%lld", (long long)st.st_size);
        struct passwd *pw = getpwuid(st.st_uid);
        struct group  *gr = getgrgid(st.st_gid);
        char ubuf[16], gbuf[16];
        if (pw) snprintf(ubuf, sizeof(ubuf), "%s", pw->pw_name);
        else    snprintf(ubuf, sizeof(ubuf), "%u", st.st_uid);
        if (gr) snprintf(gbuf, sizeof(gbuf), "%s", gr->gr_name);
        else    snprintf(gbuf, sizeof(gbuf), "%u", st.st_gid);
        printf("%s %2lu %-8s %-8s %8s %s\n",
               m, (unsigned long)st.st_nlink, ubuf, gbuf, szbuf, name);
    } else if (flag_1) {
        puts(name);
    } else {
        printf("%s  ", name);
    }
}

/* comparator for qsort on char* array */
static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int list_dir(const char *path, bool head)
{
    DIR *d = opendir(path); if (!d) { kx_warn(path); return 1; }
    if (head) printf("%s:\n", path);

    struct dirent *de;
    char **names = NULL; size_t n = 0, cap = 0;
    while ((de = readdir(d))) {
        if (!flag_a && de->d_name[0] == '.') continue;
        if (n == cap) {
            cap = cap ? cap*2 : 32;
            char **tmp = realloc(names, cap * sizeof(*names));
            if (!tmp) { closedir(d); free(names); return 1; }
            names = tmp;
        }
        names[n++] = strdup(de->d_name);
    }
    closedir(d);
    qsort(names, n, sizeof(*names), cmp_str);
    for (size_t i = 0; i < n; i++) print_entry(path, names[i]);
    if (!flag_l && !flag_1) putchar('\n');

    if (flag_R) {
        for (size_t i = 0; i < n; i++) {
            char child[PATH_MAX];
            snprintf(child, sizeof(child), "%s/%s", path, names[i]);
            struct stat st;
            if (lstat(child, &st) == 0 && S_ISDIR(st.st_mode) &&
                strcmp(names[i], ".") != 0 && strcmp(names[i], "..") != 0) {
                putchar('\n');
                list_dir(child, true);
            }
        }
    }
    for (size_t i = 0; i < n; i++) free(names[i]);
    free(names);
    return 0;
}

static int list_one(const char *path)
{
    struct stat st;
    if (lstat(path, &st) < 0) { kx_warn(path); return 1; }
    if (!S_ISDIR(st.st_mode)) {
        /* single file */
        if (flag_l) {
            char m[11]; mode_string(st.st_mode, m);
            char szbuf[32];
            if (flag_h) human_size((long long)st.st_size, szbuf, sizeof(szbuf));
            else snprintf(szbuf, sizeof(szbuf), "%lld", (long long)st.st_size);
            struct passwd *pw = getpwuid(st.st_uid);
            struct group  *gr = getgrgid(st.st_gid);
            char ubuf[16], gbuf[16];
            if (pw) snprintf(ubuf, sizeof(ubuf), "%s", pw->pw_name);
            else    snprintf(ubuf, sizeof(ubuf), "%u", st.st_uid);
            if (gr) snprintf(gbuf, sizeof(gbuf), "%s", gr->gr_name);
            else    snprintf(gbuf, sizeof(gbuf), "%u", st.st_gid);
            printf("%s %2lu %-8s %-8s %8s %s\n",
                   m, (unsigned long)st.st_nlink, ubuf, gbuf, szbuf, kx_base(path));
        } else if (flag_1) {
            puts(kx_base(path));
        } else {
            printf("%s\n", kx_base(path));
        }
        return 0;
    }
    return list_dir(path, false);
}

int main(int argc, char **argv)
{
    kx_prog = "ls";
    int first = 1;
    for (; first < argc && argv[first][0] == '-' && argv[first][1]; first++) {
        if (strcmp(argv[first], "--") == 0) { first++; break; }
        for (char *o = argv[first]+1; *o; o++) {
            switch (*o) {
            case 'l': flag_l = true; break;
            case 'a': flag_a = true; break;
            case 'R': flag_R = true; break;
            case 'h': flag_h = true; break;
            case '1': flag_1 = true; break;
            default:
                fprintf(stderr, "%s: invalid option -%c\n", kx_prog, *o); exit(1);
            }
        }
    }
    if (first == argc) return list_one(".");
    int rc = 0;
    bool multi = argc - first > 1;
    for (int i = first; i < argc; i++) {
        if (multi) { printf("%s:\n", argv[i]); }
        struct stat st;
        if (lstat(argv[i], &st) == 0 && S_ISDIR(st.st_mode))
            rc |= list_dir(argv[i], false);
        else
            rc |= list_one(argv[i]);
        if (multi && i+1 < argc) putchar('\n');
    }
    return rc;
}
