/* Copyright (c) 2020 The Linux Foundation. All rights reserved.
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
*/

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <log/logger.h>
#include "ota_utils.h"

#define OTA_STATUS_LEN 12
static const char *OTA_STATUS_COOKIE_FILE = "/cache/recovery/ota_status";

int get_ota_status() {
    int fd = -1;
    int status = OTA_STATUS_UNKNOWN;
    char buf[OTA_STATUS_LEN] = { 0 };
    fd = open(OTA_STATUS_COOKIE_FILE, O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        printf("Failed to open %s : %s\n",
             OTA_STATUS_COOKIE_FILE,
             strerror(errno));
        return status;
    }
    int len = read(fd, buf, OTA_STATUS_LEN - 1);
    if (len < 0) {
        printf("Failed to read to %s : %s\n", OTA_STATUS_COOKIE_FILE,
             strerror(errno));
        close(fd);
        return status;
    }
    printf("ota status %s\n", buf);
    if (!strncmp(buf, "OTA_STARTED", strlen("OTA_STARTED"))) {
        status = OTA_STATUS_STARTED;
    } else if (!strncmp(buf, "OTA_SUCCESS", strlen("OTA_SUCCESS"))) {
        status = OTA_STATUS_SUCCESS;
    } else if (!strncmp(buf, "OTA_FAILED", strlen("OTA_FAILED"))) {
        status = OTA_STATUS_FAILED;
    } else {
        status = OTA_STATUS_UNKNOWN;
    }
    printf("ota status value %d\n", status);
    close(fd);
    return status;
}
