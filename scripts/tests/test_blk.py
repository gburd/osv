# Copyright (C) 2026 Greg Burd
# BSD license; see LICENSE in the top-level directory.
from tests.testing import *
import os
import tempfile

arch = os.uname().machine
def set_arch(_arch):
    global arch
    arch = _arch

# Run tst-blk-all-devices on a second NVMe controller, so that every per-CPU
# IO queue of the NVMe driver gets a request.  The root disk is NVMe as well
# (an NVMe disk next to a virtio-blk one gets a clashing vblk name), so the
# guest boots with -k.
@test
def blk_all_devices_nvme():
    global arch
    run_args = []
    if os.uname().machine != arch:
        run_args = ['--arch', arch]
    with tempfile.NamedTemporaryFile(prefix='osv-blk-', suffix='.img') as disk:
        disk.truncate(64 << 20)
        guest = Guest(['-k', '-N', '--second-nvme-image', disk.name,
                       '-e', '/tests/tst-blk-all-devices.so /dev/vblk1'],
                      run_py_args=run_args)
        wait_for_line(guest, 'PASS')
        guest.join()
