/*
 * Copyright (C) 2026 Greg Burd
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

/*
 * Diagnosis only (wip/walflush-diag, never in a PR): counters and log2(us)
 * latency histograms for the fsync/fdatasync/sync_file_range path, the vnode
 * lock around write() and fsync(), pagecache::writeback_inode(), and (from
 * libsolaris.so, which calls these kernel entry points) the ZIL commit phases.
 */
#ifndef OSV_WALFLUSH_DIAG_H
#define OSV_WALFLUSH_DIAG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Record one duration (ns) into histogram slot `id`. */
void walflush_diag_ns(int id, uint64_t ns);
/* Add `v` to counter `id`. */
void walflush_diag_count(int id, uint64_t v);
/* The whole dump, for whygap-ctl/walflush-ctl ("dump"). */
int walflush_diag_dump(char *buf, int len);
uint64_t walflush_diag_now_ns(void);

/* Histogram ids (kernel side). */
enum {
    WFD_FSYNC_WAL,          /* sys_fsync total, file under pg_wal/ */
    WFD_FSYNC_OTHER,        /* sys_fsync total, any other file */
    WFD_WB_INODE,           /* writeback_inode total (incl. write_lock wait) */
    WFD_WB_LOCKWAIT,        /* writeback_inode write_lock acquisition */
    WFD_FSYNC_VNLOCK_WAL,   /* sys_fsync vn_lock acquisition, WAL */
    WFD_FSYNC_VNLOCK_OTHER, /* sys_fsync vn_lock acquisition, other */
    WFD_VOP_FSYNC_WAL,      /* VOP_FSYNC under vn_lock, WAL */
    WFD_VOP_FSYNC_OTHER,    /* VOP_FSYNC under vn_lock, other */
    WFD_WRITE_VNLOCK_WAL,   /* vfs_file::write vn_lock acquisition, WAL */
    WFD_WRITE_VNLOCK_OTHER, /* vfs_file::write vn_lock acquisition, other */
    WFD_WRITE_WAL,          /* vfs_file::write write_file() under lock, WAL */
    WFD_READ_VNLOCK,        /* vfs_file::read vn_lock acquisition (all files) */
    WFD_SFR,                /* sync_file_range total */
    /* ZFS side (libsolaris.so, OpenZFS patch 0099-walflush-diag). */
    WFD_ZC_TOTAL,           /* zil_commit_impl total */
    WFD_ZC_A2S,             /* zil_async_to_sync */
    WFD_ZC_WRITER,          /* zil_commit_writer (issuer lock wait + build + issue) */
    WFD_ZC_ISSUER_WAIT,     /* zl_issuer_lock acquisition in zil_commit_writer */
    WFD_ZC_WAITER,          /* zil_commit_waiter */
    WFD_ZC_WAKE,            /* zil_commit_waiter_done() -> waiter running again */
    WFD_ZC_TXGWAIT,         /* txg_wait_synced fallback after the waiter */
    WFD_LWB_IO,             /* lwb issue -> flush_vdevs_done */
    WFD_TXG_SYNC,           /* spa_sync of one txg */
    /* OpenZFS patch 0100 (the throttle, indirect-write and lwb-size questions). */
    WFD_TX_WAIT,            /* dmu_tx_wait() total (dirty throttle + delay) */
    WFD_TX_DELAY,           /* dmu_tx_delay() sleep */
    WFD_GET_DATA,           /* zfs_get_data() for one itx (incl. dmu_sync issue) */
    WFD_UNUSED_25,          /* reserved (keeps the patch 0100 ids stable) */
    WFD_LWB_SIZE,           /* lwb_sz at issue, in "ns" units = bytes */
    /* OpenZFS patch 0101: one indirect WAL block in zfs_get_data() / dmu_sync(). */
    WFD_GD_ZGET,            /* zfs_zget + generation check */
    WFD_GD_RANGELOCK,       /* zfs_rangelock_enter on the block (RL_READER) */
    WFD_GD_BUFHOLD,         /* dmu_buf_hold_noread */
    WFD_GD_DMU_SYNC,        /* the dmu_sync() call */
    WFD_DS_LOOKUP,          /* dmu_sync: write policy, db_mtx, dirty-record lookup */
    WFD_DS_ARC_WRITE,       /* dmu_sync: arc_write() zio setup */
    WFD_DS_NOWAIT,          /* dmu_sync: zio_nowait() (pipeline issue) */
    WFD_DS_IO,              /* dmu_sync issue -> dmu_sync_done (the block write) */
    WFD_NHIST
};

/* Counter ids (kernel side). */
enum {
    WFC_WB_CALLS,
    WFC_WB_SKIPPED,         /* write_cache empty guard fired */
    WFC_WB_PAGES_SCANNED,
    WFC_WB_PAGES_FLUSHED,
    WFC_WB_CACHE_SIZE_SUM,  /* write_cache.size() summed at each call */
    WFC_SFR_CALLS,
    WFC_WRITE_WAL_BYTES,
    WFC_ZC_TIMEOUTS,        /* zil_commit_waiter_timeout calls */
    WFC_ZC_WRITER_SKIPS,    /* writer found the waiter already on an lwb */
    WFC_LWB_BYTES,          /* lwb_nused at issue */
    WFC_ITX_COPIED,         /* zil_lwb_commit itx states */
    WFC_ITX_NEEDCOPY,
    WFC_ITX_INDIRECT,
    WFC_ITX_INDIRECT_BYTES,
    WFC_DMU_SYNC_LATE,      /* late-arrival dmu_sync() (block's txg is syncing) */
    WFC_DMU_SYNC_EEXIST,    /* block's txg already synced */
    WFC_LWBS,               /* lwbs issued */
    WFC_TX_DIRTY_OVER_MAX,  /* dmu_tx_wait() found dp_dirty_total >= zfs_dirty_data_max */
    WFC_TX_DELAYS,          /* dmu_tx_delay() actually slept */
    WFC_NCOUNT
};

#ifdef __cplusplus
}
#endif

#endif
