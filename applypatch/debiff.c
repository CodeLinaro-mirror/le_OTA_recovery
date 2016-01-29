/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

// Debiff format:
// Header (8 bytes): DEBIFF10
// AR archive containing the control.tar and data.tar binary patches

#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mincrypt/sha.h"
#include "applypatch.h"
#include "utils.h"

#define MAX_PATH_LENGTH 32
#define DEBIFF_HEADER_STRING "DEBIFF10"

#define LOGD(...) \
  do \
  { \
  fprintf(stdout, "DEBUG:%s:%d: ",__FUNCTION__,__LINE__); \
  fprintf(stdout, __VA_ARGS__); \
  fprintf(stdout, "\n"); \
  } while(0)


static long long Read8_be(void* pv) {
    unsigned char* p = pv;
    return (long long)(((unsigned long long)p[0] << 56) |
                       ((unsigned long long)p[1] << 48) |
                       ((unsigned long long)p[2] << 40) |
                       ((unsigned long long)p[3] << 32) |
                       ((unsigned long long)p[4] << 24) |
                       ((unsigned long long)p[5] << 16) |
                       ((unsigned long long)p[6] << 8) |
                       (unsigned long long)p[7]);
}

/**
 * @brief Writes a set of data to the file descriptor
 *
 * @param[in] file_descriptor The file descriptor to write the data to
 * @param[in] data Pointer to the data to write
 * @param[in] data_size Size of the data to write
 *
 * @return int Number of bytes written on success, negative error code
 *         otherwise
 */
static int write_data_to_file_descriptor(int file_descriptor, const char* data,
                                         ssize_t data_size)
{
    int result;
    size_t pos = 0;
    size_t bytes_written;
    size_t total_bytes_written = 0;
    size_t bytes_remaining = 0;

    bytes_remaining = data_size;

    // Write the data
    do {
        bytes_written = write(file_descriptor, data + pos, bytes_remaining);
        if (bytes_written < 0) {
            LOGD("Unable to write data - %s", strerror(errno));
            result = bytes_written;
            goto bailout;
        }
        // If we didn't write all the data then we loop back to complete
        // writing it
        pos += bytes_written;
        bytes_remaining -= bytes_written;
        total_bytes_written += bytes_written;
    } while (bytes_written > 0 && bytes_remaining > 0);

    if (total_bytes_written != data_size) {
        LOGD("Mismatch in size between data size %u and total bytes wrote %u",
             data_size, total_bytes_written);
        result = -EIO;
        goto bailout;
    }

    result = bytes_written;

bailout:
    return result;
}

/**
 * @brief Writes a datagram to a file
 *
 * @param[in] filename The filename to be open and written to
 * @param[in] data The data to be written. The data must be of the format
 *       where the first 8 bytes is the length of the entire data to be
 *       written
 *
 * @return int The number of bytes written on success, negative error code on
 *         failure
 */
static int write_data_to_file(const char* filename, const char* data,
                              ssize_t data_size)
{
    int file_descriptor = -1;
    int result;

    if (filename == NULL) {
        LOGD("Invalid filename");
        return -EINVAL;
    }
    if (data == NULL) {
        LOGD("Invalid data argument");
        return -EINVAL;
    }
    if (data_size == 0) {
        LOGD("DATA SIZE is 0");
        return -EINVAL;
    }

    // Open the file
    file_descriptor = open(filename, O_RDWR | O_CREAT);
    if (file_descriptor < 0) {
        result = errno;
        LOGD("Unable to create file handle - %s", strerror(result));
        goto bailout;
    }

    result = write_data_to_file_descriptor(file_descriptor, data, data_size);
    if (result < 0) {
        LOGD("Error writing data to file descriptor: %d", result);
        goto bailout;
    }

bailout:

    if (file_descriptor >= 0) {
        close(file_descriptor);
        file_descriptor = -1;
    }

    return result;
}

static int read_and_patch_file(const char* file, const Value* patch)
{
    // Read in the contents to memory
    struct stat st;
    int result;
    long fsize;
    char *data_buf = NULL;
    unsigned char* new_data = NULL;
    ssize_t new_size = 0;
    size_t read_items = 0;
    FILE *fp = NULL;

    if (file == NULL || patch == NULL) {
        LOGD("Invalid argument passed");
        return -EINVAL;
    }

    result = stat(file, &st);
    if (result != 0) {
        LOGD("Unable to stat %s - %s", file, strerror(errno));
        return -EIO;
    }

    fsize = st.st_size;

    data_buf = malloc(fsize);
    if (data_buf == NULL) {
        LOGD("Unable to allocate buffer of size %d", fsize);
        return -ENOMEM;
    }

    fp = fopen(file, "rb");
    if (fp == NULL) {
        LOGD("Unable to open %s file", file);
        return -ENOENT;
    }

    read_items = fread(data_buf, 1, fsize, fp);
    if (read_items < fsize) {
        LOGD("Error reading in %s", file);
        return -EIO;
    }
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }

    // Patch the data in memory
    result = ApplyBSDiffPatchMem(data_buf, fsize, patch, 0, &new_data, &new_size);

    if (result != 0) {
        LOGD("Error applying bsdiff patch");
        return -EFAULT;
    }

    result = unlink(file);
    if (result != 0) {
        LOGD("Unable to delete %s - ", file, strerror(errno));
        return result;
    }

    result = write_data_to_file(file, new_data, new_size);
    if (result < 0) {
        LOGD("Unable to write patched data for %s", file);
        return -EIO;
    }

    result = 0;

    return result;
}


/*
 * Apply the patch given in 'patch_filename' to the source data given
 * by (old_data, old_size).  Write the patched output to the 'output'
 * file, and update the SHA context with the output data as well.
 * Return 0 on success.
 */
int ApplyDebiffPatch(const unsigned char* old_data, ssize_t old_size,
                     const Value* patch,
                     SinkFn sink, void* token, SHA_CTX* ctx) {
    int ret = 0;
    int result;
    size_t pos = 0;
    char* header = patch->data;
    size_t bytes;
    Value control_patch;
    Value data_patch;

    if (patch->size < 8) {
        LOGD("patch too short to contain header");
        return -1;
    }

    if (memcmp(header, DEBIFF_HEADER_STRING, strlen(DEBIFF_HEADER_STRING)) != 0) {
        LOGD("corrupt patch file header (magic number)");
        return -1;
    }

    // Advance position pointer past DEBIFF10 header (length 8)
    pos += 8;

    // Create temp folder
    char dir_template[] = "/tmp/tmpXXXXXX";
    char *dir_location = NULL;
    dir_location = mkdtemp(dir_template);
    if (dir_location == NULL) {
        LOGD("Couldn't create temporary directory: %s", strerror(errno));
        return -1;
    }

    // Change work directory to temp folder
    char *prev_dir = NULL;
    // Prev_dir is dynamically allocated in getcwd
    prev_dir = getcwd(prev_dir, 0);
    if (prev_dir == NULL) {
        LOGD("Unable to save previously working directory: %s", strerror(errno));
        return -1;
    }

    result = chdir(dir_location);
    if (result != 0) {
        LOGD("Unable to change to new directory: %s", strerror(errno));
        return -1;
    }

    // First in the patch is the control patch information.
    // Read the amount of data we need to write (8 bytes)
    control_patch.size = Read8_be(patch->data+pos);
    if (control_patch.size < 0) {
        LOGD("Control patch size is invalid!");
        return -1;
    }
    // We need to advance beyond the data length and store the pointer to
    // the patch data
    pos += 8;
    control_patch.data = patch->data+pos;
    control_patch.type = VAL_BLOB;

    // Advance the number of bytes in the control patch data
    pos += control_patch.size;

    // Data patch information is next. First read the length of the data patch
    // into memory
    data_patch.size = Read8_be(patch->data+pos);
    if (data_patch.size < 0) {
        LOGD("Data patch size is invalid!");
        return -1;
    }
    // Advance past the length and then set the location of the data
    pos += 8;
    data_patch.data = patch->data+pos;
    data_patch.type = VAL_BLOB;

    // Save the old file
    result = write_data_to_file("old_file.deb", old_data, old_size);
    if (result < 0) {
        return result;
    }

    // Extract control.tar and data.tar from source deb using dpkg-deb
    ret = system("dpkg-deb --fsys-tarfile old_file.deb > data.tar");
    if (ret) {
        LOGD("Couldn't run dpkg-deb --fsys-tarfile");
        return -1;
    }

    result = read_and_patch_file("data.tar", &data_patch);
    if (result != 0) {
        LOGD("Unable to successfully read and patch data.tar");
        return result;
    }

    ret = system("dpkg-deb --control-tarfile old_file.deb > control.tar");
    if (ret) {
        LOGD("Couldn't run dpkg-deb --control-tarfile");
        return -1;
    }

    result = read_and_patch_file("control.tar", &control_patch);
    if (result != 0) {
        LOGD("Unable to successfully read and patch data.tar");
        return result;
    }

    // Finally, use dpkg-deb to build the new deb file
    ret = system("dpkg-deb --tar-build control.tar data.tar new_file.deb");
    if (ret) {
        LOGD("Couldn't build the new deb file");
        return -1;
    }

    // Copy contents to destination file
    struct stat st;
    stat("new_file.deb", &st);
    long fsize = st.st_size;

    char *new_file_buf = malloc(fsize);
    if (new_file_buf == NULL) {
        LOGD("Unable to allocate buffer of size %d", fsize);
        return -ENOMEM;
    }

    FILE *new_file_fp = fopen("new_file.deb", "rb");
    if (new_file_fp == NULL) {
        LOGD("Unable to open new file");
        return -1;
    }

    fread(new_file_buf, fsize, 1, new_file_fp);
    if (new_file_fp != NULL) {
        fclose(new_file_fp);
        new_file_fp = NULL;
    }

    result = write_data_to_file_descriptor(*(int*)token, new_file_buf, fsize);
    if (result < 0) {
        LOGD("Unable to write data to file descriptor: %d", result);
        return result;
    }

    // Calculate the sha1 on the final data
    SHA_update(ctx, new_file_buf, fsize);

    free(new_file_buf);
    new_file_buf = NULL;

    // Go back to the original directory
    chdir(prev_dir);

    // Free prev which was dynamically allocated in getcwd()
    free(prev_dir);

    // Delete the temporary folder
    // We can use a fixed file path because we know dir_location
    // will be no longer than /tmp/tmpXXXXXX
    char delete_command[MAX_PATH_LENGTH];
    sprintf(delete_command, "rm -rf %s", dir_location);
    system(delete_command);

    return 0;
}
