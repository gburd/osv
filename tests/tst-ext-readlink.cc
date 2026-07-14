/*
 * Copyright (C) 2026 Greg Burd
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

// Regression test for the ext_readlink hardening: a normal symlink must still
// read back correctly, and the bounds guards must not break it.  This test is
// self-contained: it creates its own symlinks on the (writable) ext root and
// exercises BOTH the fast path (short target stored inline in the inode) and
// the slow path (long target stored in a data block), then cleans up.  It runs
// only in the ext test image (ext-only-tests in modules/tests/Makefile).
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static int failures = 0;

// Verify readlink(@path) returns exactly @want.
static void check_link(const char *path, const char *want)
{
    char buf[512];
    memset(buf, 0, sizeof(buf));
    ssize_t n = readlink(path, buf, sizeof(buf) - 1);
    if (n < 0) {
        fprintf(stderr, "FAIL: readlink(%s): %s\n", path, strerror(errno));
        failures++;
        return;
    }
    buf[n] = 0;
    if ((size_t)n != strlen(want) || strcmp(buf, want) != 0) {
        fprintf(stderr, "FAIL: readlink(%s) = '%s' (%zd), expected '%s' (%zu)\n",
                path, buf, n, want, strlen(want));
        failures++;
        return;
    }
    fprintf(stderr, "PASS: readlink(%s) = '%s'\n", path, buf);
}

int main()
{
    fprintf(stderr, "=== tst-ext-readlink ===\n");

    // Fast path: a short target (<60 bytes) is stored inline in the inode.
    static const char *fast_link = "/tst-ext-readlink-fast";
    static const char *fast_tgt  = "realfile";

    // Slow path: a long target (>60 bytes) is stored in a data block.
    static const char *slow_link = "/tst-ext-readlink-slow";
    static const char slow_tgt[] =
        "a/very/long/symlink/target/that/exceeds/the/sixty/byte/inline/"
        "inode/storage/threshold/so/it/lands/in/a/data/block";

    // Clean up any leftovers from a previous run so creation is deterministic.
    unlink(fast_link);
    unlink(slow_link);

    if (symlink(fast_tgt, fast_link) != 0) {
        fprintf(stderr, "FAIL: symlink(%s): %s\n", fast_link, strerror(errno));
        failures++;
    } else {
        check_link(fast_link, fast_tgt);
    }

    if (symlink(slow_tgt, slow_link) != 0) {
        fprintf(stderr, "FAIL: symlink(%s): %s\n", slow_link, strerror(errno));
        failures++;
    } else {
        check_link(slow_link, slow_tgt);
    }

    unlink(fast_link);
    unlink(slow_link);

    fprintf(stderr, "=== tst-ext-readlink done: %d failures ===\n", failures);
    return failures == 0 ? 0 : 1;
}
