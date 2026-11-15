/*
 * Copyright (C) 2026 Greg Burd
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

// Objects a fork child loads are global: every process finds them in the
// module list.  So everything the load allocates for them must be readable
// from every address space, after the loading process is gone.
//  - "dlopen" (the default): a child dlopen()s a library and exits without
//    closing it.  The parent then walks the module list (dl_iterate_phdr,
//    reading each object's name and program headers), forks again, and the
//    new child walks it too.  8 rounds.
//  - "execve": 500 rounds of fork() + execve() of a payload, which loads the
//    payload in the exec'd thread; the parent reaps it and checks its exit
//    status.  Each exec runs in a new ELF namespace, so this also needs the
//    namespace of an exited program to be reused.  A second argument sets the
//    number of rounds; more than the 2046 namespaces fails if exec'd programs
//    are never released.

#include <osv/kernel_config_fork.h>

#include <cstdio>

#if !CONF_fork

int main()
{
    printf("SKIP: tst-fork-dlopen requires CONF_fork\n");
    return 0;
}

#else

#include <dlfcn.h>
#include <errno.h>
#include <link.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *lib = "/tests/libforksym.so";

struct walk {
    int objects;
    int found;
    unsigned long sum;
};

static int visit(struct dl_phdr_info *info, size_t, void *data)
{
    auto w = static_cast<walk*>(data);
    w->objects++;
    if (info->dlpi_name && strstr(info->dlpi_name, "libforksym.so")) {
        w->found++;
    }
    for (int i = 0; i < info->dlpi_phnum; i++) {
        w->sum += info->dlpi_phdr[i].p_type + info->dlpi_phdr[i].p_memsz;
    }
    w->sum += info->dlpi_name ? strlen(info->dlpi_name) : 0;
    return 0;
}

static walk walk_modules()
{
    walk w{};
    dl_iterate_phdr(visit, &w);
    return w;
}

static int reap(pid_t c)
{
    int st = 0;
    if (c < 0 || waitpid(c, &st, 0) != c || !WIFEXITED(st)) {
        return -1;
    }
    return WEXITSTATUS(st);
}

static int run_dlopen()
{
    int failures = 0;
    for (int round = 0; round < 8; round++) {
        pid_t c = fork();
        if (c == 0) {
            void *h = dlopen(lib, RTLD_NOW);
            if (!h) {
                _exit(2);
            }
            auto fn = reinterpret_cast<long (*)(long)>(dlsym(h, "forksym_fn_1"));
            _exit(fn && fn(20) == 41 ? 0 : 3);
        }
        int r = reap(c);
        if (r) {
            printf("FAIL: round %d: the loading child exited %d\n", round, r);
            failures++;
            break;
        }
        walk w = walk_modules();
        pid_t g = fork();
        if (g == 0) {
            walk wg = walk_modules();
            _exit(wg.objects == w.objects && wg.sum == w.sum ? 0 : 4);
        }
        r = reap(g);
        if (r) {
            printf("FAIL: round %d: a later child's module walk differs (exit %d)\n", round, r);
            failures++;
            break;
        }
        if (round == 0) {
            printf("dlopen: %d objects, the library %s\n", w.objects,
                   w.found ? "listed" : "not listed");
        }
    }
    return failures;
}

static int run_execve(int rounds)
{
    int bad = 0;
    for (int i = 0; i < rounds; i++) {
        pid_t c = fork();
        if (c == 0) {
            char *const argv[] = { (char*)"/tests/payload-exit7.so", nullptr };
            char *const envp[] = { nullptr };
            execve(argv[0], argv, envp);
            _exit(99);
        }
        if (reap(c) != 7) {
            bad++;
        }
    }
    printf("execve: %d rounds, %d with the wrong exit status\n", rounds, bad);
    if (bad) {
        printf("FAIL: fork + execve\n");
    }
    return bad != 0;
}

int main(int argc, char **argv)
{
    errno = 0;
    pid_t probe = fork();
    if (probe == -1 && errno == ENOSYS) {
        printf("SKIP: fork() not enabled\n");
        return 0;
    }
    if (probe == 0) {
        _exit(0);
    }
    waitpid(probe, nullptr, 0);
    bool exec = argc > 1 && !strcmp(argv[1], "execve");
    int rounds = exec && argc > 2 ? atoi(argv[2]) : 500;
    int failures = exec ? run_execve(rounds) : run_dlopen();
    if (!failures) {
        printf("PASS: tst-fork-dlopen %s\n", exec ? "execve" : "dlopen");
    }
    printf("SUMMARY: %d failures\n", failures);
    return failures ? 1 : 0;
}

#endif
