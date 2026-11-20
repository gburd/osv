/*
 * Copyright (C) 2026 Greg Burd
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

// Diagnosis only (wip/walflush-diag): see include/osv/walflush_diag.h.
// Every slot is a relaxed atomic, so the cost on the measured path is one
// clock read per timestamp and a few uncontended-in-the-common-case adds.

#include <atomic>
#include <stdio.h>
#include <osv/clock.hh>
#include <osv/export.h>
#include <osv/walflush_diag.h>

namespace {

constexpr int nbucket = 20;   // log2(us): bucket b holds [2^(b-1), 2^b) us, b=0 < 1 us

struct hist {
    std::atomic<uint64_t> n{0}, ns{0}, max{0};
    std::atomic<uint64_t> b[nbucket] {};
};

hist hists[WFD_NHIST];
std::atomic<uint64_t> counts[WFC_NCOUNT];

const char *hist_name[WFD_NHIST] = {
    "fsync_wal", "fsync_other", "wb_inode", "wb_lockwait",
    "fsync_vnlock_wal", "fsync_vnlock_other", "vop_fsync_wal", "vop_fsync_other",
    "write_vnlock_wal", "write_vnlock_other", "write_wal", "read_vnlock", "sfr",
    "zc_total", "zc_a2s", "zc_writer", "zc_issuer_wait", "zc_waiter", "zc_wake",
    "zc_txgwait", "lwb_io", "txg_sync", "tx_wait", "tx_delay", "get_data", "unused25",
    "lwb_size",
};
const char *count_name[WFC_NCOUNT] = {
    "wb_calls", "wb_skipped", "wb_pages_scanned", "wb_pages_flushed",
    "wb_cache_size_sum", "sfr_calls", "write_wal_bytes", "zc_timeouts",
    "zc_writer_skips", "lwb_bytes", "itx_copied", "itx_needcopy", "itx_indirect", "itx_indirect_bytes",
    "dmu_sync_late", "dmu_sync_eexist", "lwbs", "tx_dirty_over_max", "tx_delays",
};

static_assert(sizeof(hist_name) / sizeof(hist_name[0]) == WFD_NHIST, "hist names");
// OpenZFS patch 0099 hard-codes these ids (C, no OSv headers there).
static_assert(WFD_ZC_TOTAL == 13 && WFD_LWB_IO == 20 && WFD_TXG_SYNC == 21, "zfs hist ids");
static_assert(WFC_ZC_TIMEOUTS == 7 && WFC_LWB_BYTES == 9, "zfs count ids");
static_assert(WFD_TX_WAIT == 22 && WFD_LWB_SIZE == 26, "zfs hist ids, patch 0100");
static_assert(WFC_ITX_COPIED == 10 && WFC_TX_DELAYS == 18, "zfs count ids, patch 0100");
static_assert(sizeof(count_name) / sizeof(count_name[0]) == WFC_NCOUNT, "count names");

}

extern "C" OSV_LIBSOLARIS_API uint64_t walflush_diag_now_ns(void)
{
    return osv::clock::uptime::now().time_since_epoch().count();
}

extern "C" OSV_LIBSOLARIS_API void walflush_diag_ns(int id, uint64_t ns)
{
    auto &h = hists[id];
    h.n.fetch_add(1, std::memory_order_relaxed);
    h.ns.fetch_add(ns, std::memory_order_relaxed);
    uint64_t m = h.max.load(std::memory_order_relaxed);
    while (ns > m && !h.max.compare_exchange_weak(m, ns, std::memory_order_relaxed)) {
    }
    int b = 0;
    while (b < nbucket - 1 && (ns >> 10) >= (1ull << b)) {
        b++;
    }
    h.b[b].fetch_add(1, std::memory_order_relaxed);
}

extern "C" OSV_LIBSOLARIS_API void walflush_diag_count(int id, uint64_t v)
{
    counts[id].fetch_add(v, std::memory_order_relaxed);
}

// libsolaris.so (patch 0100) registers a printer for the ZFS tunables and the pool's dirty totals.
static std::atomic<int (*)(char *, int)> zfs_state_dump{nullptr};

extern "C" OSV_LIBSOLARIS_API void walflush_diag_set_zfs_dump(int (*fn)(char *, int))
{
    zfs_state_dump.store(fn);
}

// One "wfh <name> n= ns= max= h=b0,b1,..." line per histogram, one "wfc" line
// for the counters.
extern "C" OSV_LIBC_API int walflush_diag_dump(char *buf, int len)
{
    int n = 0;
    for (int i = 0; i < WFD_NHIST && n < len - 512; i++) {
        auto &h = hists[i];
        n += snprintf(buf + n, len - n, "wfh %s n=%lu ns=%lu max=%lu h=", hist_name[i],
                      h.n.load(), h.ns.load(), h.max.load());
        for (int b = 0; b < nbucket; b++) {
            n += snprintf(buf + n, len - n, "%s%lu", b ? "," : "", h.b[b].load());
        }
        n += snprintf(buf + n, len - n, "\n");
    }
    n += snprintf(buf + n, len - n, "wfc");
    for (int i = 0; i < WFC_NCOUNT && n < len - 64; i++) {
        n += snprintf(buf + n, len - n, " %s=%lu", count_name[i], counts[i].load());
    }
    n += snprintf(buf + n, len - n, "\n");
    auto zd = zfs_state_dump.load();
    if (zd && n < len - 2048) {
        n += zd(buf + n, len - n);
    }
    return n;
}
