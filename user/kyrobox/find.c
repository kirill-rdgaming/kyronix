#include "common.h"
#include <fnmatch.h>
#include <sys/wait.h>

#define MAX_DEPTH 128

static const char *name_pat;
static int  type_filter = 0; /* 0=any, 'd'=dir, 'f'=reg, 'l'=sym */
static int  max_depth = MAX_DEPTH;
static char **exec_argv; /* -exec CMD {} ; */
static int   exec_argc;

static bool type_match(const struct stat *st)
{
    if (!type_filter) return true;
    switch (type_filter) {
    case 'f': return S_ISREG(st->st_mode);
    case 'd': return S_ISDIR(st->st_mode);
    case 'l': return S_ISLNK(st->st_mode);
    default:  return false;
    }
}

static void do_exec(const char *path)
{
    /* build argv replacing {} with path */
    char **av = malloc((size_t)(exec_argc+1) * sizeof(char *));
    if (!av) return;
    for (int i = 0; i < exec_argc; i++)
        av[i] = strcmp(exec_argv[i], "{}") == 0 ? (char *)path : exec_argv[i];
    av[exec_argc] = NULL;
    pid_t pid = fork();
    if (pid == 0) { execvp(av[0], av); _exit(127); }
    if (pid > 0) waitpid(pid, NULL, 0);
    free(av);
}

static int walk(const char *path, int depth)
{
    if (depth > max_depth) return 0;
    struct stat st;
    if (lstat(path, &st) < 0) { kx_warn(path); return 1; }
    if ((!name_pat || fnmatch(name_pat, kx_base(path), 0) == 0) && type_match(&st)) {
        if (exec_argv) do_exec(path);
        else puts(path);
    }
    if (!S_ISDIR(st.st_mode)) return 0;
    DIR *d = opendir(path); if (!d) { kx_warn(path); return 1; }
    int rc = 0; struct dirent *de;
    while ((de = readdir(d))) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        char child[PATH_MAX];
        snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
        rc |= walk(child, depth+1);
    }
    closedir(d); return rc;
}

int main(int argc, char **argv)
{
    kx_prog = "find";
    const char *root = ".";
    int i = 1;
    if (i < argc && argv[i][0] != '-') root = argv[i++];
    while (i < argc) {
        if (strcmp(argv[i], "-name") == 0 && i+1 < argc) {
            name_pat = argv[++i]; i++;
        } else if (strcmp(argv[i], "-type") == 0 && i+1 < argc) {
            type_filter = argv[++i][0]; i++;
        } else if (strcmp(argv[i], "-maxdepth") == 0 && i+1 < argc) {
            max_depth = atoi(argv[++i]); i++;
        } else if (strcmp(argv[i], "-exec") == 0) {
            i++;
            exec_argv = &argv[i];
            while (i < argc && strcmp(argv[i], ";") != 0) i++;
            exec_argc = (int)(&argv[i] - exec_argv);
            if (i < argc) i++; /* skip ; */
        } else {
            fprintf(stderr, "%s: unknown predicate: %s\n", kx_prog, argv[i]); exit(1);
        }
    }
    return walk(root, 0);
}
