#include "common.h"

static bool opt_recursive;
static bool opt_force;

static int join_path(char *out, size_t outsz, const char *dir, const char *base)
{
    int n = snprintf(out, outsz, "%s%s%s", dir, dir[0] && dir[strlen(dir) - 1] == '/' ? "" : "/", base);
    if (n < 0 || (size_t)n >= outsz) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

static int copy_one(const char *src, const char *dst);

static int copy_dir(const char *src, const char *dst, mode_t mode)
{
    DIR *d = opendir(src);
    if (!d)
        return -1;
    if (mkdir(dst, mode & 07777) < 0 && errno != EEXIST) {
        closedir(d);
        return -1;
    }

    int rc = 0;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        char s[PATH_MAX], t[PATH_MAX];
        if (join_path(s, sizeof(s), src, de->d_name) < 0 ||
            join_path(t, sizeof(t), dst, de->d_name) < 0) {
            kx_warn(src);
            rc = -1;
            continue;
        }
        if (copy_one(s, t) < 0)
            rc = -1;
    }
    closedir(d);
    chmod(dst, mode & 07777);
    return rc;
}

static int copy_regular(const char *src, const char *dst, mode_t mode)
{
    if (opt_force)
        unlink(dst);
    if (kx_copy_file(src, dst) < 0)
        return -1;
    chmod(dst, mode & 07777);
    return 0;
}

static int copy_symlink(const char *src, const char *dst)
{
    char target[PATH_MAX];
    ssize_t n = readlink(src, target, sizeof(target) - 1);
    if (n < 0)
        return -1;
    target[n] = '\0';
    if (opt_force)
        unlink(dst);
    return symlink(target, dst);
}

static int copy_one(const char *src, const char *dst)
{
    struct stat st;
    if (lstat(src, &st) < 0) {
        kx_warn(src);
        return -1;
    }
    if (S_ISDIR(st.st_mode)) {
        if (!opt_recursive) {
            fprintf(stderr, "%s: %s: is a directory\n", kx_prog, src);
            return -1;
        }
        if (copy_dir(src, dst, st.st_mode) < 0) {
            kx_warn(src);
            return -1;
        }
        return 0;
    }
    if (S_ISLNK(st.st_mode)) {
        if (copy_symlink(src, dst) < 0) {
            kx_warn(src);
            return -1;
        }
        return 0;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "%s: %s: unsupported file type\n", kx_prog, src);
        return -1;
    }
    if (copy_regular(src, dst, st.st_mode) < 0) {
        kx_warn(src);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    kx_prog = "cp";
    int first = 1;
    for (; first < argc && argv[first][0] == '-' && argv[first][1]; first++) {
        if (strcmp(argv[first], "--") == 0) { first++; break; }
        for (const char *p = argv[first] + 1; *p; p++) {
            if (*p == 'r' || *p == 'R') opt_recursive = true;
            else if (*p == 'f') opt_force = true;
            else kx_die("usage: cp [-fRr] SRC... DST");
        }
    }
    if (argc - first < 2)
        kx_die("usage: cp [-fRr] SRC... DST");

    const char *dst = argv[argc - 1];
    struct stat dst_st;
    bool dst_is_dir = stat(dst, &dst_st) == 0 && S_ISDIR(dst_st.st_mode);
    if (argc - first > 2 && !dst_is_dir)
        kx_die("target is not a directory");

    int rc = 0;
    for (int i = first; i < argc - 1; i++) {
        char target[PATH_MAX];
        const char *to = dst;
        if (dst_is_dir) {
            if (join_path(target, sizeof(target), dst, kx_base(argv[i])) < 0) {
                kx_warn(dst);
                rc = 1;
                continue;
            }
            to = target;
        }
        if (copy_one(argv[i], to) < 0)
            rc = 1;
    }
    return rc;
}
