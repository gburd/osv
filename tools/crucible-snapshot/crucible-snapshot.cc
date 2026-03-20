/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

/**
 * Crucible Snapshot Tool
 *
 * Creates snapshots on Crucible block devices.
 *
 * Usage: crucible-snapshot <device> <snapshot_id>
 *
 * Example:
 *   crucible-snapshot /dev/crucible0 12345
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

// Crucible ioctl command (must match crucible-blk.hh)
#define CRUCIBLE_IOC_CREATE_SNAPSHOT  _IOW('C', 1, uint64_t)

static void usage(const char* prog)
{
    fprintf(stderr, "Usage: %s <device> <snapshot_id>\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Create a snapshot on a Crucible block device.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "  device       Path to Crucible block device (e.g., /dev/crucible0)\n");
    fprintf(stderr, "  snapshot_id  Numeric snapshot identifier (0-18446744073709551615)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s /dev/crucible0 1\n", prog);
    fprintf(stderr, "  %s /dev/crucible0 20240320\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Notes:\n");
    fprintf(stderr, "  - Snapshots require all 3 downstairs servers (3/3 quorum)\n");
    fprintf(stderr, "  - The device must be writable (not read-only)\n");
    fprintf(stderr, "  - Snapshot IDs must be unique across the region\n");
    exit(1);
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        usage(argv[0]);
    }

    const char* device = argv[1];
    const char* snapshot_id_str = argv[2];

    // Parse snapshot ID
    char* endptr = nullptr;
    unsigned long long snapshot_id_ull = strtoull(snapshot_id_str, &endptr, 10);

    if (*endptr != '\0' || snapshot_id_str[0] == '\0') {
        fprintf(stderr, "Error: Invalid snapshot ID '%s' (must be numeric)\n", snapshot_id_str);
        return 1;
    }

    uint64_t snapshot_id = static_cast<uint64_t>(snapshot_id_ull);

    printf("Creating snapshot on %s with ID %lu\n", device, snapshot_id);

    // Open device
    int fd = open(device, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Error: Failed to open device '%s': %s\n", device, strerror(errno));
        return 1;
    }

    // Issue ioctl to create snapshot
    int result = ioctl(fd, CRUCIBLE_IOC_CREATE_SNAPSHOT, &snapshot_id);

    if (result < 0) {
        fprintf(stderr, "Error: Failed to create snapshot: %s\n", strerror(errno));
        fprintf(stderr, "\n");

        if (errno == EROFS) {
            fprintf(stderr, "The device is read-only. Snapshots require write access.\n");
        } else if (errno == EIO) {
            fprintf(stderr, "I/O error. Possible causes:\n");
            fprintf(stderr, "  - Not all 3 downstairs servers are connected\n");
            fprintf(stderr, "  - Network error during snapshot operation\n");
            fprintf(stderr, "  - Downstairs server rejected the snapshot\n");
        } else if (errno == ENODEV) {
            fprintf(stderr, "Device not available or Crucible client not initialized.\n");
        }

        close(fd);
        return 1;
    }

    close(fd);

    printf("SUCCESS: Snapshot %lu created on %s\n", snapshot_id, device);
    printf("\n");
    printf("The snapshot is now available on all 3 downstairs servers.\n");

    return 0;
}
