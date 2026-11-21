/*
 * Copyright (C) 2026 Greg Burd
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

// After setvbuf(stdout, NULL, _IONBF, 0), stdout is unbuffered: each write
// must reach file descriptor 1 at once, with nothing left in the stream's
// buffer.  This test writes through puts(), fputs(), fwrite() and fputc()
// in turn, and after each one checks with __fpending() that no bytes are
// pending.  On OSv the first such write stays in the buffer, and since
// nothing flushes stdout when main() returns, the line is lost.
//
// The results go to stderr through write(), so they do not depend on the
// stream under test.

#include <stdio.h>
#include <stdio_ext.h>
#include <string.h>
#include <unistd.h>

static int failures;

static void report(bool ok, const char *what, size_t pending)
{
    char line[160];
    int n = snprintf(line, sizeof line, "%s: %s (%zu bytes pending)\n",
                     ok ? "PASS" : "FAIL", what, pending);
    write(2, line, n);
    if (!ok) {
        failures++;
    }
}

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);

    puts("tst-stdout-unbuffered: line written with puts()");
    size_t p = __fpending(stdout);
    report(p == 0, "puts() on an unbuffered stdout leaves nothing pending", p);

    fputs("tst-stdout-unbuffered: line written with fputs()\n", stdout);
    p = __fpending(stdout);
    report(p == 0, "fputs() on an unbuffered stdout leaves nothing pending", p);

    const char w[] = "tst-stdout-unbuffered: line written with fwrite()\n";
    fwrite(w, 1, strlen(w), stdout);
    p = __fpending(stdout);
    report(p == 0, "fwrite() on an unbuffered stdout leaves nothing pending", p);

    fputc('\n', stdout);
    p = __fpending(stdout);
    report(p == 0, "fputc() on an unbuffered stdout leaves nothing pending", p);

    char line[64];
    int n = snprintf(line, sizeof line, "SUMMARY: %d failures\n", failures);
    write(2, line, n);
    return failures ? 1 : 0;
}
