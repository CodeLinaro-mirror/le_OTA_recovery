/**********************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*********************************************************/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <set>
#include <cerrno>
#include <ctime>
#include "openssl/sha.h"
#ifdef TARGET_SUPPORTS_AB
#include <abctl/libabctl.h>
#endif

#define BOOTDEVICE_DIR "/dev/disk/by-partlabel"
#define LOG_PATH "/mnt/userdata/cache/recovery/mirror_copy.log"
#define STATUS_FILE "/mnt/userdata/cache/recovery/mirror_copy_status"
#define BUFFER_SIZE (4096*1024) //

#define STATUS_NOT_STARTED "NOT_STARTED"
#define STATUS_STARTED "STARTED"
#define STATUS_COMPLETED "COMPLETED"
#define STATUS_FAILED "FAILED"

const char* slot_suffix[] = {"", "_b", NULL};

// Save original stdout/stderr for emergency fallback
static int saved_stdout = -1;
static int saved_stderr = -1;

// {{ Helper: Write status to cookie file with robust error handling }}
void write_status_file(const char* status) {
    int fd = open(STATUS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        ssize_t len = strlen(status);
        ssize_t written = write(fd, status, len);
        if (written != len) {
            // Log error but continue - best effort
            fprintf(stderr, "ERROR: Failed to write complete status to file: %s\n", strerror(errno));
        }
        close(fd);
    }
}

// {{ Helper: Convert SHA1 hash to hex string }}
static std::string sha1_to_string(const unsigned char* hash) {
    char buf[SHA_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        snprintf(buf + i * 2, sizeof(buf) - i * 2, "%02x", hash[i]);
    }
    buf[SHA_DIGEST_LENGTH * 2] = '\0';  // Ensure null-termination
    return std::string(buf, SHA_DIGEST_LENGTH * 2);
}

void restore_original_output() {
    if (saved_stdout != -1) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        saved_stdout = -1;
    }
    if (saved_stderr != -1) {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
        saved_stderr = -1;
    }
}

bool setup_logging() {
    // Save original stdout/stderr first
    saved_stdout = dup(STDOUT_FILENO);
    saved_stderr = dup(STDERR_FILENO);
    if (saved_stdout < 0 || saved_stderr < 0) {
        // Critical failure - cannot proceed safely
        if (saved_stdout >= 0) close(saved_stdout);
        if (saved_stderr >= 0) close(saved_stderr);
        return false;
    }

    // Create directory if needed (best effort)
    const char* dir_path = "/mnt/userdata/cache/recovery";
    mkdir(dir_path, 0755);

    // Open log file with append mode
    int log_fd = open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd < 0) {
        // Fallback to original stderr
        restore_original_output();
        fprintf(stderr, "ERROR: Failed to open log file '%s': %s\n",
                LOG_PATH, strerror(errno));
        return false;
    }

    // Redirect stdout/stderr to log file
    if (dup2(log_fd, STDOUT_FILENO) < 0 ||
        dup2(log_fd, STDERR_FILENO) < 0) {
        close(log_fd);
        restore_original_output();
        fprintf(stderr, "ERROR: Failed to redirect streams: %s\n", strerror(errno));
        return false;
    }

    // Close our copy of the log fd (dup2 takes ownership)
    close(log_fd);

    // Set line buffering for immediate log visibility
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    // Log initialization
    time_t now = time(NULL);
    char* ts = ctime(&now);
    if (ts) ts[strlen(ts)-1] = '\0'; // Remove newline
    fprintf(stdout, "[%s] mirror_copy started\n", ts);

    // {{ Set initial status }}
    write_status_file(STATUS_STARTED);
    return true;
}

// {{ Enhanced block copy with SHA1 verification }}
bool copy_block_device(const char* src, const char* dst) {
    int src_fd = -1;
    int dest_fd = -1;
    bool success = true;
    char* buffer = nullptr;

    // Initialize SHA contexts
    SHA_CTX src_ctx, dest_ctx;
    unsigned char src_hash[SHA_DIGEST_LENGTH];
    unsigned char dest_hash[SHA_DIGEST_LENGTH];

    // Open source device
    src_fd = open(src, O_RDONLY);
    if (src_fd == -1) {
        fprintf(stderr, "ERROR: Failed to open source device '%s': %s\n",
                src, strerror(errno));
        return false;
    }

    // Open destination device with O_RDWR for verification
    dest_fd = open(dst, O_RDWR);
    if (dest_fd == -1) {
        fprintf(stderr, "ERROR: Failed to open destination device '%s': %s\n",
                dst, strerror(errno));
        close(src_fd);
        return false;
    }

    // Allocate buffer
    buffer = static_cast<char*>(malloc(BUFFER_SIZE));
    if (!buffer) {
        fprintf(stderr, "ERROR: Memory allocation failed\n");
        goto cleanup;
    }

    // Initialize SHA contexts
    SHA1_Init(&src_ctx);
    SHA1_Init(&dest_ctx);

    // Read from source, update SHA, write to destination
    ssize_t nread;
    while ((nread = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
        // Update source SHA
        SHA1_Update(&src_ctx, buffer, nread);

        // Write to destination with partial write handling
        ssize_t nwritten = 0;
        while (nwritten < nread) {
            ssize_t result = write(dest_fd, buffer + nwritten, nread - nwritten);
            if (result <= 0) {
                if (result == 0) {
                    fprintf(stderr, "ERROR: Unexpected EOF during write to '%s'\n", dst);
                } else {
                    fprintf(stderr, "ERROR: Write failed at offset %zd: %s\n",
                            nwritten, strerror(errno));
                }
                success = false;
                goto cleanup;
            }
            nwritten += result;
        }
    }

    if (nread < 0) {
        fprintf(stderr, "ERROR: Read failed from '%s': %s\n", src, strerror(errno));
        success = false;
        goto cleanup;
    }

    // Finalize source SHA
    SHA1_Final(src_hash, &src_ctx);

    // Ensure data is fully written to disk
    if (fsync(dest_fd) != 0) {
        fprintf(stderr, "ERROR: fsync failed for '%s': %s\n", dst, strerror(errno));
        success = false;
        goto cleanup;
    }

    // Reset file offset to beginning for verification
    if (lseek(dest_fd, 0, SEEK_SET) == -1) {
        fprintf(stderr, "ERROR: lseek failed for '%s': %s\n", dst, strerror(errno));
        success = false;
        goto cleanup;
    }

    // Compute SHA of destination by reading back
    while ((nread = read(dest_fd, buffer, BUFFER_SIZE)) > 0) {
        SHA1_Update(&dest_ctx, buffer, nread);
    }
    SHA1_Final(dest_hash, &dest_ctx);

    // Fixed: Properly scoped hash comparison to avoid goto issues
    {
        // Compare hashes
        bool verification_success = (memcmp(src_hash, dest_hash, SHA_DIGEST_LENGTH) == 0);

        // Log hash comparison results
        std::string src_hash_str = sha1_to_string(src_hash);
        std::string dest_hash_str = sha1_to_string(dest_hash);
        fprintf(stdout, "Source hash: %s\n", src_hash_str.c_str());
        fprintf(stdout, "Dest hash: %s\n", dest_hash_str.c_str());
        fprintf(stdout, "Hash %s\n", verification_success ? "MATCH" : "MISMATCH");

        success = verification_success;
    }

cleanup:
    // Clean up resources
    if (buffer) free(buffer);
    if (src_fd >= 0) close(src_fd);
    if (dest_fd >= 0) close(dest_fd);

    return success;
}

int main(int argc, char** argv) {
    // {{ Setup logging before any output }}
    if (!setup_logging()) {
        // Critical failure - cannot log
        // {{ Still update status file }}
        write_status_file(STATUS_FAILED);
        return 1;
    }

    // {{ Determine partitions to copy (default to system,efi) }}
    std::vector<std::string> partitions;
    std::set<std::string> seen;

    auto add_partition = [&](const std::string& p) {
        if (!p.empty() && seen.find(p) == seen.end()) {
            seen.insert(p);
            partitions.push_back(p);
        }
    };

    if (argc == 1) {
        // Default partitions when no arguments provided
        add_partition("system");
        add_partition("efi");
    } else {
        // Parse user-specified partitions
        char* partitions_str = strdup(argv[1]);
        if (!partitions_str) {
            fprintf(stderr, "ERROR: Memory allocation failed\n");
            // {{ Update status file }}
            write_status_file(STATUS_FAILED);
            return 1;
        }

        char* token = strtok(partitions_str, ",");
        while (token) {
            // {{ FIXED: Secure whitespace trimming with bounds checking }}
            char* start = token;
            while (*start == ' ') start++;
            size_t len = strlen(start);
            if (len == 0) {
                token = strtok(NULL, ",");
                continue;
            }

            char* end = start + len - 1;
            while (end >= start && (*end == ' ' || *end == '\n')) {
                *end-- = '\0';
            }

            add_partition(start);
            token = strtok(NULL, ",");
        }
        free(partitions_str);
    }

    // {{ Validate at least one partition specified }}
    if (partitions.empty()) {
        fprintf(stderr, "ERROR: No partitions specified to copy\n");
        // {{ Update status file }}
        write_status_file(STATUS_FAILED);
        return 1;
    }

    // {{ Get current slot using libabctl with proper error handling }}
    int current_slot = -1;
#ifdef TARGET_SUPPORTS_AB
    current_slot = libabctl_getBootSlot();
    if (current_slot < 0) {
        fprintf(stderr, "ERROR: Failed to get current boot slot\n");
        // {{ Update status file }}
        write_status_file(STATUS_FAILED);
        return 1;
    }
#else
    fprintf(stderr, "ERROR: This device does not support A/B slots\n");
    // {{ Update status file }}
    write_status_file(STATUS_FAILED);
    return 1;
#endif

    int inactive_slot = (current_slot == 0) ? 1 : 0;

    // {{ Track which partitions we find }}
    std::vector<bool> found(partitions.size(), false);
    bool copy_success = true;

    // {{ Process block devices }}
    DIR* dir = opendir(BOOTDEVICE_DIR);
    if (!dir) {
        fprintf(stderr, "ERROR: opendir(%s) failed: %s\n",
                BOOTDEVICE_DIR, strerror(errno));
        // {{ Update status file }}
        write_status_file(STATUS_FAILED);
        return 1;
    }

    struct dirent* de;
    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0] == '.') continue;

        char active_path[PATH_MAX];
        snprintf(active_path, sizeof(active_path),
                "%s/%s", BOOTDEVICE_DIR, de->d_name);

        struct stat st;
        if (stat(active_path, &st) != 0) {
            fprintf(stderr, "WARNING: stat(%s) failed: %s\n",
                    active_path, strerror(errno));
            continue;
        }
        if (!S_ISBLK(st.st_mode)) continue;

        // {{ Identify active slot partition }}
        const char* suffix = slot_suffix[current_slot];
        size_t suffix_len = strlen(suffix);
        size_t name_len = strlen(de->d_name);
        bool is_active = false;
        std::string base_name;

        if (suffix_len == 0) {
            is_active = true;
            base_name = de->d_name;
        }
        else if (name_len > suffix_len &&
                 strcmp(de->d_name + name_len - suffix_len, suffix) == 0) {
            is_active = true;
            base_name = std::string(de->d_name, name_len - suffix_len);
        }

        if (!is_active) continue;

        // {{ Check if partition needs copying }}
        for (size_t i = 0; i < partitions.size(); i++) {
            if (base_name == partitions[i]) {
                found[i] = true;

                // {{ Prepare inactive slot path }}
                std::string inactive_name = base_name + slot_suffix[inactive_slot];
                char inactive_path[PATH_MAX];
                snprintf(inactive_path, sizeof(inactive_path),
                        "%s/%s", BOOTDEVICE_DIR, inactive_name.c_str());

                struct stat st_inactive;
                if (stat(inactive_path, &st_inactive) != 0) {
                    fprintf(stderr, "ERROR: stat(%s) failed: %s\n",
                            inactive_path, strerror(errno));
                    copy_success = false;
                    goto cleanup;
                }
                if (!S_ISBLK(st_inactive.st_mode)) {
                    fprintf(stderr, "ERROR: %s is not a block device\n", inactive_path);
                    copy_success = false;
                    goto cleanup;
                }

                // {{ Execute copy operation }}
                fprintf(stdout, "Copying %s [%s → %s]\n", base_name.c_str(),
                        active_path, inactive_path);
                if (!copy_block_device(active_path, inactive_path)) {
                    fprintf(stderr, "ERROR: Copy failed for %s\n", base_name.c_str());
                    copy_success = false;
                    goto cleanup;
                }
                break; // Found matching partition, move to next device
            }
        }
    }

    // {{ Verify all requested partitions were found }}
    for (size_t i = 0; i < partitions.size(); i++) {
        if (!found[i]) {
            fprintf(stderr, "ERROR: Partition '%s' not found in active slot\n",
                    partitions[i].c_str());
            copy_success = false;
        }
    }

cleanup:
    closedir(dir);
    // {{ Update status file based on final result }}
    if (copy_success) {
        write_status_file(STATUS_COMPLETED);
    } else {
        write_status_file(STATUS_FAILED);
    }

    return copy_success ? 0 : 1;
}
