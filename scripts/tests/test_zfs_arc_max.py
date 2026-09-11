# Copyright (C) 2026 Greg Burd
#
# This work is open source software, licensed under the terms of the
# BSD license as described in the LICENSE file in the top-level directory.

# --zfs-arc-max must bound the ZFS ARC.  Boot the probe on a ZFS-root image
# with the default ceiling (most of the 2 GB guest) and with
# --zfs-arc-max=64M.  The probe reads a 192 MiB file of incompressible data
# and reports the free memory the reads used: the default ARC keeps all of
# it, the capped one at most 64 MiB.

from tests.testing import *
import os
import re

arch = os.uname().machine
def set_arch(_arch):
    global arch
    arch = _arch

def probe_used(kernel_args):
    run_args = []
    if os.uname().machine != arch:
        run_args = ['--arch', arch]
    guest = Guest(['-e', kernel_args + '/tests/misc-zfs-arc-probe.so'], run_py_args=run_args)
    line = wait_for_line_starts(guest, 'arc-probe ')
    guest.join()
    m = re.match(r'arc-probe size=(\d+) used=(-?\d+)$', line)
    assert m, line
    return int(m.group(1)), int(m.group(2))

@test
def zfs_arc_max():
    manifest = os.path.join(osv_base, 'build/last/usr.manifest')
    with open(manifest) as f:
        if '/tests/misc-zfs-arc-probe.so' not in f.read():
            print('SKIP (not a ZFS image)', end=' ')
            return
    # The first boot may have to write the file; measure after it exists.
    probe_used('')
    size, used_default = probe_used('')
    _, used_capped = probe_used('--zfs-arc-max=64M ')
    mb = 1 << 20
    print('file %d MB, ARC used %d MB default, %d MB capped' %
          (size // mb, used_default // mb, used_capped // mb), end=' ')
    assert used_default > size * 3 // 4, 'the default ARC did not cache the file'
    assert used_capped < used_default - (size - 64 * mb) // 2
