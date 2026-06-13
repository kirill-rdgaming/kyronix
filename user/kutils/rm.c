#include "common.h"

static bool opt_force;
static bool opt_recursive;

static int rm_one(const char *path)
{
    struct stat st;
    if (lstat(path, &st) < 0) {
        if (opt_force && errno == ENOENT)
            return 0;
        kx_warn(path);
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        if (!opt_recursive) {
            fprintf(stderr, "%s: %s: is a directory\n", kx_prog, path);
            return -1;
        }
        DIR *d = opendir(path);
        if (!d) {
            kx_warn(path);
            return -1;
        }
        int rc = 0;
        struct dirent *de;
        while ((de = readdir(d))) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            char child[PATH_MAX];
            int n = snprintf(child, sizeof(child), "%s%s%s", path,
                             path[0] && path[strlen(path) - 1] == '/' ? "" : "/", de->d_name);
            if (n < 0 || (size_t)n >= sizeof(child)) {
                errno = ENAMETOOLONG;
                kx_warn(path);
                rc = -1;
                continue;
            }
            if (rm_one(child) < 0)
                rc = -1;
        }
        closedir(d);
        if (rmdir(path) < 0) {
            if (!(opt_force && errno == ENOENT))
                kx_warn(path);
            return -1;
        }
        return rc;
    }

    if (unlink(path) < 0) {
        if (opt_force && errno == ENOENT)
            return 0;
        kx_warn(path);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    kx_prog = "rm";
    int first = 1;
    for (; first < argc && argv[first][0] == '-' && argv[first][1]; first++) {
        if (strcmp(argv[first], "--") == 0) { first++; break; }
        for (const char *p = argv[first] + 1; *p; p++) {
            if (*p == 'f') opt_force = true;
            else if (*p == 'r' || *p == 'R') opt_recursive = true;
            else kx_die("usage: rm [-fRr] FILE...");
        }
    }
    if (first == argc && !opt_force) kx_die("missing operand");
    int rc = 0;
    for (int i = first; i < argc; i++) {
        if (rm_one(argv[i]) < 0)
            rc = 1;
    }
    return rc;
}
