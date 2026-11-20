/*
 * Copyright (C) 2026 Greg Burd
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 *
 * walflush-probe (diagnosis only): exercise the instrumented paths on a ZFS
 * file system and print walflush_diag_dump() before and after.
 *   probe <dir> [nthreads] [secs] [sfr_every] [mmapcheck]
 * sfr_every 0 = no sync_file_range; mmapcheck 0 skips the mmap check (it
 * leaves pages in the write cache, which disables the fsync scan guard).
 * Each thread appends 8 kB to <dir>/pg_wal/seg<i> and fdatasyncs it, while one
 * more thread writes 8 kB pages to <dir>/rel and sync_file_range()s every
 * 32 pages (PG's checkpoint_flush_after), so the WAL and other-file splits and
 * the ZIL histograms all move.  Also mmaps a file MAP_SHARED, dirties it,
 * fsyncs and verifies (the writeback_inode non-skip path).
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int walflush_diag_dump(char *buf, int len) __attribute__((weak));

static char dumpbuf[1 << 16];
static const char *dir;
static int secs = 5;
static int sfr_every = 32;
static volatile int stop;

static void dump(const char *tag)
{
    if (!walflush_diag_dump) {
        printf("PROBE no walflush_diag_dump\n");
        return;
    }
    walflush_diag_dump(dumpbuf, sizeof(dumpbuf));
    printf("=== %s\n%s", tag, dumpbuf);
}

static void *wal_writer(void *arg)
{
    char path[512], buf[8192];
    snprintf(path, sizeof(path), "%s/pg_wal/seg%ld", dir, (long)arg);
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (fd < 0) {
        perror(path);
        return NULL;
    }
    memset(buf, 'w', sizeof(buf));
    long n = 0;
    while (!stop) {
        if (pwrite(fd, buf, sizeof(buf), (n % 2048) * sizeof(buf)) != sizeof(buf) || fdatasync(fd)) {
            perror("wal");
            break;
        }
        n++;
    }
    close(fd);
    printf("PROBE wal%ld ops=%ld\n", (long)arg, n);
    return NULL;
}

static void *rel_writer(void *arg)
{
    char path[512], buf[8192];
    (void)arg;
    snprintf(path, sizeof(path), "%s/rel", dir);
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0600);
    memset(buf, 'r', sizeof(buf));
    long n = 0;
    while (!stop && fd >= 0) {
        pwrite(fd, buf, sizeof(buf), (n % 16384) * sizeof(buf));
        n++;
        if (sfr_every && n % sfr_every == 0) {
            sync_file_range(fd, 0, 0, SYNC_FILE_RANGE_WRITE);
        }
    }
    close(fd);
    printf("PROBE rel ops=%ld\n", n);
    return NULL;
}

/* Dirty a MAP_SHARED mapping, fsync, then read() it back through a new fd. */
static int mmap_check(void)
{
    char path[512];
    const int npages = 16;
    snprintf(path, sizeof(path), "%s/mm", dir);
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (fd < 0 || ftruncate(fd, npages * 4096)) {
        return 1;
    }
    unsigned char *p = mmap(NULL, npages * 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        return 1;
    }
    for (int i = 0; i < npages; i++) {
        memset(p + i * 4096, 0x40 + i, 4096);
    }
    int rc = fsync(fd);
    munmap(p, npages * 4096);
    close(fd);
    fd = open(path, O_RDONLY);
    unsigned char b[4096];
    for (int i = 0; i < npages && !rc; i++) {
        if (pread(fd, b, 4096, i * 4096) != 4096 || b[0] != 0x40 + i || b[4095] != 0x40 + i) {
            rc = 1;
        }
    }
    close(fd);
    return rc;
}

int main(int argc, char **argv)
{
    char path[512];
    dir = argc > 1 ? argv[1] : "/tmp";
    int nt = argc > 2 ? atoi(argv[2]) : 4;
    secs = argc > 3 ? atoi(argv[3]) : 5;
    sfr_every = argc > 4 ? atoi(argv[4]) : 32;
    mkdir(dir, 0700);
    snprintf(path, sizeof(path), "%s/pg_wal", dir);
    mkdir(path, 0700);
    dump("before");
    if (argc <= 5 || atoi(argv[5])) {
        printf("PROBE mmapcheck %s\n", mmap_check() ? "FAIL" : "PASS");
    }
    dump("after-mmap");
    pthread_t t[65];
    nt = nt > 64 ? 64 : nt;
    for (long i = 0; i < nt; i++) {
        pthread_create(&t[i], NULL, wal_writer, (void *)i);
    }
    pthread_create(&t[nt], NULL, rel_writer, NULL);
    sleep(secs);
    stop = 1;
    for (int i = 0; i <= nt; i++) {
        pthread_join(t[i], NULL);
    }
    dump("after-load");
    printf("PROBE DONE\n");
    return 0;
}
