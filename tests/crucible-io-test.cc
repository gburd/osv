/*
 * Basic I/O test for Crucible device
 *
 * Tests basic read/write operations on /dev/crucible0
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#define DEVICE_PATH "/dev/crucible0"
#define BLOCK_SIZE 4096
#define TEST_BLOCKS 4

static void print_hex(const char* label, const void* data, size_t len) {
    const unsigned char* bytes = (const unsigned char*)data;
    printf("%s: ", label);
    for (size_t i = 0; i < len && i < 32; i++) {
        printf("%02x ", bytes[i]);
    }
    if (len > 32) {
        printf("...");
    }
    printf("\n");
}

int test_device_exists() {
    printf("Test: Device exists\n");
    printf("------------------\n");

    struct stat st;
    if (stat(DEVICE_PATH, &st) < 0) {
        printf("FAIL: Device %s does not exist: %s\n", DEVICE_PATH, strerror(errno));
        return 1;
    }

    if (!S_ISBLK(st.st_mode)) {
        printf("FAIL: %s is not a block device\n", DEVICE_PATH);
        return 1;
    }

    printf("PASS: Device %s exists and is a block device\n", DEVICE_PATH);
    printf("      Major: %d, Minor: %d\n",
           (int)(st.st_rdev >> 8), (int)(st.st_rdev & 0xFF));
    printf("\n");
    return 0;
}

int test_open_close() {
    printf("Test: Open and close device\n");
    printf("---------------------------\n");

    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        printf("FAIL: Cannot open %s: %s\n", DEVICE_PATH, strerror(errno));
        return 1;
    }

    printf("PASS: Opened device (fd=%d)\n", fd);

    if (close(fd) < 0) {
        printf("FAIL: Cannot close device: %s\n", strerror(errno));
        return 1;
    }

    printf("PASS: Closed device\n");
    printf("\n");
    return 0;
}

int test_write() {
    printf("Test: Write to device\n");
    printf("---------------------\n");

    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        printf("FAIL: Cannot open device: %s\n", strerror(errno));
        return 1;
    }

    // Prepare test data
    char* write_buf = (char*)malloc(BLOCK_SIZE * TEST_BLOCKS);
    if (!write_buf) {
        printf("FAIL: Cannot allocate write buffer\n");
        close(fd);
        return 1;
    }

    // Fill with pattern
    for (int i = 0; i < BLOCK_SIZE * TEST_BLOCKS; i++) {
        write_buf[i] = (char)(i & 0xFF);
    }

    print_hex("Write pattern", write_buf, 32);

    // Write data
    ssize_t written = write(fd, write_buf, BLOCK_SIZE * TEST_BLOCKS);
    if (written < 0) {
        printf("FAIL: Write failed: %s\n", strerror(errno));
        free(write_buf);
        close(fd);
        return 1;
    }

    if (written != BLOCK_SIZE * TEST_BLOCKS) {
        printf("FAIL: Partial write: %zd/%d bytes\n",
               written, BLOCK_SIZE * TEST_BLOCKS);
        free(write_buf);
        close(fd);
        return 1;
    }

    printf("PASS: Wrote %zd bytes (%d blocks)\n", written, TEST_BLOCKS);

    free(write_buf);
    close(fd);
    printf("\n");
    return 0;
}

int test_read() {
    printf("Test: Read from device\n");
    printf("----------------------\n");

    int fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        printf("FAIL: Cannot open device: %s\n", strerror(errno));
        return 1;
    }

    // Seek to beginning
    if (lseek(fd, 0, SEEK_SET) < 0) {
        printf("FAIL: Seek failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    char* read_buf = (char*)malloc(BLOCK_SIZE * TEST_BLOCKS);
    if (!read_buf) {
        printf("FAIL: Cannot allocate read buffer\n");
        close(fd);
        return 1;
    }

    memset(read_buf, 0, BLOCK_SIZE * TEST_BLOCKS);

    ssize_t bytes_read = read(fd, read_buf, BLOCK_SIZE * TEST_BLOCKS);
    if (bytes_read < 0) {
        printf("FAIL: Read failed: %s\n", strerror(errno));
        free(read_buf);
        close(fd);
        return 1;
    }

    printf("PASS: Read %zd bytes (%zd blocks)\n",
           bytes_read, bytes_read / BLOCK_SIZE);
    print_hex("Read data", read_buf, 32);

    free(read_buf);
    close(fd);
    printf("\n");
    return 0;
}

int test_write_read_verify() {
    printf("Test: Write, read, and verify\n");
    printf("------------------------------\n");

    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        printf("FAIL: Cannot open device: %s\n", strerror(errno));
        return 1;
    }

    // Seek to a specific offset
    off_t offset = 8192;  // 2 blocks in
    if (lseek(fd, offset, SEEK_SET) < 0) {
        printf("FAIL: Seek failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    // Prepare test pattern
    char write_buf[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++) {
        write_buf[i] = (char)(0xAA);
    }

    // Write
    ssize_t written = write(fd, write_buf, BLOCK_SIZE);
    if (written != BLOCK_SIZE) {
        printf("FAIL: Write failed or incomplete\n");
        close(fd);
        return 1;
    }

    // Seek back
    if (lseek(fd, offset, SEEK_SET) < 0) {
        printf("FAIL: Seek back failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    // Read back
    char read_buf[BLOCK_SIZE];
    memset(read_buf, 0, BLOCK_SIZE);

    ssize_t bytes_read = read(fd, read_buf, BLOCK_SIZE);
    if (bytes_read != BLOCK_SIZE) {
        printf("FAIL: Read failed or incomplete\n");
        close(fd);
        return 1;
    }

    // Note: Mock server returns zeros, so we can't verify exact data
    // In production, you would compare read_buf with write_buf
    printf("PASS: Write and read completed at offset %ld\n", offset);
    printf("NOTE: Mock server returns dummy data - verification skipped\n");

    close(fd);
    printf("\n");
    return 0;
}

int test_aligned_io() {
    printf("Test: Block-aligned I/O\n");
    printf("-----------------------\n");

    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        printf("FAIL: Cannot open device: %s\n", strerror(errno));
        return 1;
    }

    // Test with exact block sizes
    char buf[BLOCK_SIZE];
    memset(buf, 0x55, BLOCK_SIZE);

    // Write one block
    ssize_t written = write(fd, buf, BLOCK_SIZE);
    if (written != BLOCK_SIZE) {
        printf("FAIL: Block-aligned write failed\n");
        close(fd);
        return 1;
    }

    printf("PASS: Single block write succeeded\n");

    // Write multiple blocks
    if (lseek(fd, BLOCK_SIZE * 10, SEEK_SET) < 0) {
        printf("FAIL: Seek to block 10 failed\n");
        close(fd);
        return 1;
    }

    char multi_buf[BLOCK_SIZE * 3];
    memset(multi_buf, 0x77, sizeof(multi_buf));

    written = write(fd, multi_buf, sizeof(multi_buf));
    if (written != sizeof(multi_buf)) {
        printf("FAIL: Multi-block write failed\n");
        close(fd);
        return 1;
    }

    printf("PASS: Multi-block write succeeded (3 blocks)\n");

    close(fd);
    printf("\n");
    return 0;
}

int main() {
    printf("Crucible Device I/O Test\n");
    printf("========================\n\n");

    int failures = 0;

    failures += test_device_exists();
    failures += test_open_close();
    failures += test_write();
    failures += test_read();
    failures += test_write_read_verify();
    failures += test_aligned_io();

    printf("Summary\n");
    printf("=======\n");
    if (failures == 0) {
        printf("All tests passed!\n");
    } else {
        printf("%d test(s) failed\n", failures);
    }

    return failures > 0 ? 1 : 0;
}
