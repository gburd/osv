/*
 * Copyright (C) 2026 Greg Burd
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

// Read a 192 MiB file of incompressible data three times, so that a ZFS root
// caches as much of it as the ARC ceiling allows, then print how much free
// memory the reads used up.  scripts/tests/test_zfs_arc_max.py boots this
// with and without --zfs-arc-max and compares the two numbers.
//
// The file is written on the first run and then kept, so that later runs
// only read; a boot that finds it already there measures a cold cache too.

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>

static const char *path = "/tmp/misc-zfs-arc-probe.dat";
static const size_t file_size = 192 << 20;
static char buf[1 << 20];

static unsigned long long free_bytes()
{
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        perror("sysinfo");
        return 0;
    }
    return (unsigned long long)info.freeram * info.mem_unit;
}

// Write the file with xorshift output, which no compressor shrinks.
static bool create_file()
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open for write");
        return false;
    }
    uint64_t x = 88172645463325252ULL;
    for (size_t off = 0; off < file_size; off += sizeof(buf)) {
        for (size_t i = 0; i < sizeof(buf) / sizeof(x); i++) {
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            reinterpret_cast<uint64_t *>(buf)[i] = x;
        }
        if (write(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf)) {
            perror("write");
            close(fd);
            return false;
        }
    }
    bool ok = fsync(fd) == 0;
    close(fd);
    return ok;
}

int main()
{
    struct stat st;
    if ((stat(path, &st) != 0 || (size_t)st.st_size != file_size) && !create_file()) {
        return 1;
    }

    unsigned long long before = free_bytes();
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    for (int pass = 0; pass < 3; pass++) {
        for (size_t off = 0; off < file_size; off += sizeof(buf)) {
            if (pread(fd, buf, sizeof(buf), off) != (ssize_t)sizeof(buf)) {
                perror("pread");
                return 1;
            }
        }
    }
    close(fd);
    unsigned long long after = free_bytes();
    printf("arc-probe size=%zu used=%lld\n", file_size, (long long)before - (long long)after);
    return 0;
}
