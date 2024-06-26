/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 * Not a contribution.
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
/* Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sstream>
#include <chrono>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <base/file.h>
#include <android-base/parseint.h>
#include <base/stringprintf.h>
#include <base/strings.h>
#include <cutils/properties.h>

#include "common.h"
#include "error_code.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/SysUtil.h"
#include "minzip/Zip.h"
#include "mtdutils/mounts.h"
#include "mtdutils/mtdutils.h"
#include "roots.h"
#include "ui.h"

#ifndef USE_LE_MODE
#include "verifier.h"
#endif

extern RecoveryUI* ui;

#define ASSUMED_UPDATE_BINARY_NAME  "META-INF/com/google/android/update-binary"
#ifdef TARGET_SUPPORTS_OTA_VERIFICATION
#define SIGNATURE_FILE_NAME  "update.sig"
#define SIGNATURE_FILE  "/tmp/update.sig"
#define CMD_BUFFER_SIZE 256
#define OTA_VERIFICATION_SUCCESS "Verified OK"
#define PUBLIC_KEY "/res/public.pem"
#endif //TARGET_SUPPORTS_OTA_VERIFICATION
#define MIN(a, b) ((a) < (b) ? (a) : (b))
static constexpr const char* AB_OTA_PAYLOAD_PROPERTIES = "payload_properties.txt";
static constexpr const char* AB_OTA_PAYLOAD = "payload.bin";
#define PUBLIC_KEYS_FILE "/res/keys"
static constexpr const char* METADATA_PATH = "META-INF/com/android/metadata";
static constexpr const char* UNCRYPT_STATUS = "/cache/recovery/uncrypt_status";

// Default allocation of progress bar segments to operations
static const int VERIFICATION_PROGRESS_TIME = 60;
static const float VERIFICATION_PROGRESS_FRACTION = 0.25;
static const float DEFAULT_FILES_PROGRESS_FRACTION = 0.4;
static const float DEFAULT_IMAGE_PROGRESS_FRACTION = 0.1;

static const char *RECOVERYUPDATER_COOKIE = "/cache/recoveryupgrade/RECOVERY_UPGRADE_DONE";
#define MANIFEST_FILE_NAME "manifest.xml"
#define MANIFEST_FILE_PATH "/tmp/manifest.xml"
#define COMMON_MANIFEST_FILE_PATH "/tmp/common_manifest.xml"
#define PATH_MAX 100
#define PRODUCT_INFO "/etc/device_info.xml"
#define COMMON_MANIFEST_FILE_NAME "manifest.xml"

//This variable holds the value of build-id fetched
static const char* buildId;

// This function parses and returns the build.version.incremental
static int parse_build_number(std::string str) {
    size_t pos = str.find("=");
    if (pos != std::string::npos) {
        std::string num_string = android::base::Trim(str.substr(pos+1));
        int build_number;
        if (android::base::ParseInt(num_string.c_str(), &build_number, 0)) {
            return build_number;
        }
    }

    LOGE("Failed to parse build number in %s.\n", str.c_str());
    return -1;
}

bool read_metadata_from_package(ZipArchive* zip, std::string* meta_data) {
    const ZipEntry* meta_entry = mzFindZipEntry(zip, METADATA_PATH);
    if (meta_entry == nullptr) {
        LOGE("Failed to find %s in update package.\n", METADATA_PATH);
        return false;
    }

    meta_data->resize(meta_entry->uncompLen, '\0');
    if (!mzReadZipEntry(zip, meta_entry, &(*meta_data)[0], meta_entry->uncompLen)) {
        LOGE("Failed to read metadata in update package.\n");
        return false;
    }
    return true;
}

#if defined(TARGET_SUPPORTS_MIRROR_AB_COPY) || defined(TARGET_SUPPORTS_MPLANE_SPEC)

const char* get_build_id_from_manifest(const ZipArchive *pArchive) {
    xmlDocPtr deviceInfo;
    xmlNodePtr deviceInfoRootNode = NULL;
    xmlNodePtr deviceInfoObjNode, deviceInfoChildNode;
    deviceInfo = xmlReadFile(PRODUCT_INFO, "UTF-8", XML_PARSE_RECOVER);
    const char *input_vendor = nullptr, *input_code = nullptr, *input_name = nullptr;
    if(NULL == deviceInfo) {
        printf("XML Document not parsed successfully.\n ");
        return nullptr;
    }

    deviceInfoRootNode = xmlDocGetRootElement(deviceInfo);
    if(NULL == deviceInfoRootNode) {
        printf("Empty XML document \n");
        xmlFreeDoc(deviceInfo);
        return nullptr;
    }
    for(deviceInfoObjNode = deviceInfoRootNode->children; deviceInfoObjNode; deviceInfoObjNode = deviceInfoObjNode->next) {
        if (deviceInfoObjNode->type != XML_ELEMENT_NODE) {
            continue;
        }

        printf("Parsing element , line %u: name=%s \n", deviceInfoObjNode->line, (char*)deviceInfoObjNode->name);
        if (deviceInfoObjNode->name != nullptr && strncmp((const char *)deviceInfoObjNode->name, "Device_info", strlen("Device_info")) == 0) {
            input_vendor = (char * ) xmlGetProp(deviceInfoObjNode, BAD_CAST "vendor");
            input_code = (char *) xmlGetProp(deviceInfoObjNode, BAD_CAST "code");
            input_name = (char *) xmlGetProp(deviceInfoObjNode, BAD_CAST "name");
        }
    }
    if(input_vendor == nullptr || input_code == nullptr || input_name == nullptr) {
        printf("input_vendor or input_code or input_name is null \n");
        return nullptr;
    }
    printf("input_vendor: %s, input_code: %s, input_name: %s", input_vendor, input_code, input_name);
    xmlDocPtr commonManifest;
    xmlNodePtr commonManifestRootNode = NULL;
    xmlNodePtr objNode, childNode;

    const ZipEntry* manifest_entry = mzFindZipEntry(pArchive, COMMON_MANIFEST_FILE_NAME);
    if (manifest_entry == NULL) {
        printf("failed to find %s\n", COMMON_MANIFEST_FILE_NAME);
        return nullptr;
    }
    const char* manifest = COMMON_MANIFEST_FILE_PATH;
    unlink(manifest);
    int manifest_fd = creat(manifest, 0644);
    if (manifest_fd < 0) {
        printf("%s: Can't make\n", manifest);
        return nullptr;
    }
    bool ok = mzExtractZipEntryToFile(pArchive, manifest_entry, manifest_fd);
    close(manifest_fd);
    if (!ok) {
        printf("%s: Can't extract from zip\n", COMMON_MANIFEST_FILE_NAME);
        return nullptr;
    }
    printf("start to load xml: %s", COMMON_MANIFEST_FILE_NAME);

    commonManifest = xmlReadFile(COMMON_MANIFEST_FILE_PATH, "UTF-8", XML_PARSE_RECOVER);
    if(NULL == commonManifest) {
        printf("XML Document not parsed successfully.\n ");
        return nullptr;
    }

    commonManifestRootNode = xmlDocGetRootElement(commonManifest);
    if(NULL == commonManifestRootNode) {
        printf("Empty XML document \n");
        xmlFreeDoc(commonManifest);
        return nullptr;
    }
    for(objNode = commonManifestRootNode->children; objNode; objNode = objNode->next) {
        if (objNode->type != XML_ELEMENT_NODE) {
            continue;
        }
        printf("Parsing element , line %u: name=%s \n", objNode->line, (char*)objNode->name);
        if (objNode->name != nullptr && strncmp((const char *)objNode->name, "products", strlen("products")) == 0) {
            for(childNode = objNode->children; childNode; childNode = childNode->next) {
                if (childNode->type != XML_ELEMENT_NODE)
                    continue;
                printf("Parsing element , line %u: name=%s \n",childNode->line, (char*)childNode->name);
                    if (childNode->name != nullptr && strncmp((const char *)childNode->name, "product", strlen("product")) == 0) {
                        const char *product_vendor = nullptr, *product_code = nullptr, *product_name = nullptr;
                        product_vendor = (char * ) xmlGetProp(childNode, BAD_CAST "vendor");
                        product_code = (char *) xmlGetProp(childNode, BAD_CAST "code");
                        product_name = (char *) xmlGetProp(childNode, BAD_CAST "name");
                        if(product_vendor != nullptr && product_code != nullptr && product_name != nullptr) {
                            if (strncmp(product_vendor, input_vendor, strlen(product_vendor)) == 0 && strncmp(product_code, input_code, strlen(product_code)) == 0 && strncmp(product_name, input_name, strlen(product_name)) == 0) {
                                return (char *) xmlGetProp(childNode, BAD_CAST "build-Id");
                            }
                        }
                    }
            }
        }
    }
    return nullptr;
}
#endif
// Read the build.version.incremental of src/tgt from the metadata and log it to last_install.
static void read_source_target_build(ZipArchive* zip, std::vector<std::string>& log_buffer) {
    std::string meta_data;
    if (!read_metadata_from_package(zip, &meta_data)) {
        return;
    }
    // Examples of the pre-build and post-build strings in metadata:
    // pre-build-incremental=2943039
    // post-build-incremental=2951741
    std::vector<std::string> lines = android::base::Split(meta_data, "\n");
    for (const std::string& line : lines) {
        std::string str = android::base::Trim(line);
        if (android::base::StartsWith(str, "pre-build-incremental")){
            int source_build = parse_build_number(str);
            if (source_build != -1) {
                log_buffer.push_back(android::base::StringPrintf("source_build: %d",
                        source_build));
            }
        } else if (android::base::StartsWith(str, "post-build-incremental")) {
            int target_build = parse_build_number(str);
            if (target_build != -1) {
                log_buffer.push_back(android::base::StringPrintf("target_build: %d",
                        target_build));
            }
        }
    }
}

// Extract the update binary from the open zip archive |zip| located at |path|
// and store into |cmd| the command line that should be called. The |status_fd|
// is the file descriptor the child process should use to report back the
// progress of the update.
static int
update_binary_command(const char* path, ZipArchive* zip, int retry_count,
                      int status_fd, std::vector<std::string>* cmd);

#ifdef AB_OTA_UPDATER

// Parses the metadata of the OTA package in |zip| and checks whether we are
// allowed to accept this A/B package. Downgrading is not allowed unless
// explicitly enabled in the package and only for incremental packages.
static int check_newer_ab_build(ZipArchive* zip)
{
    std::string metadata_str;
    if (!read_metadata_from_package(zip, &metadata_str)) {
        return INSTALL_CORRUPT;
    }
    std::map<std::string, std::string> metadata;
    for (const std::string& line : android::base::Split(metadata_str, "\n")) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            metadata[line.substr(0, eq)] = line.substr(eq + 1);
        }
    }
    char value[PROPERTY_VALUE_MAX];

    property_get("ro.product.device", value, "");
    const std::string& pkg_device = metadata["pre-device"];
    if (pkg_device != value || pkg_device.empty()) {
        LOGE("Package is for product %s but expected %s\n",
             pkg_device.c_str(), value);
        return INSTALL_ERROR;
    }

    // We allow the package to not have any serialno, but if it has a non-empty
    // value it should match.
    property_get("ro.serialno", value, "");
    const std::string& pkg_serial_no = metadata["serialno"];
    if (!pkg_serial_no.empty() && pkg_serial_no != value) {
        LOGE("Package is for serial %s\n", pkg_serial_no.c_str());
        return INSTALL_ERROR;
    }

    if (metadata["ota-type"] != "AB") {
        LOGE("Package is not A/B\n");
        return INSTALL_ERROR;
    }

    // Incremental updates should match the current build.
    property_get("ro.build.version.incremental", value, "");
    const std::string& pkg_pre_build = metadata["pre-build-incremental"];
    if (!pkg_pre_build.empty() && pkg_pre_build != value) {
        LOGE("Package is for source build %s but expected %s\n",
             pkg_pre_build.c_str(), value);
        return INSTALL_ERROR;
    }
    property_get("ro.build.fingerprint", value, "");
    const std::string& pkg_pre_build_fingerprint = metadata["pre-build"];
    if (!pkg_pre_build_fingerprint.empty() &&
        pkg_pre_build_fingerprint != value) {
        LOGE("Package is for source build %s but expected %s\n",
             pkg_pre_build_fingerprint.c_str(), value);
        return INSTALL_ERROR;
    }

    // Check for downgrade version.
    int64_t build_timestampt = property_get_int64(
            "ro.build.date.utc", std::numeric_limits<int64_t>::max());
    int64_t pkg_post_timespampt = 0;
    // We allow to full update to the same version we are running, in case there
    // is a problem with the current copy of that version.
    if (metadata["post-timestamp"].empty() ||
        !android::base::ParseInt(metadata["post-timestamp"].c_str(),
                                 &pkg_post_timespampt) ||
        pkg_post_timespampt < build_timestampt) {
        if (metadata["ota-downgrade"] != "yes") {
            LOGE("Update package is older than the current build, expected a "
                 "build newer than timestamp %" PRIu64 " but package has "
                 "timestamp %" PRIu64 " and downgrade not allowed.\n",
                 build_timestampt, pkg_post_timespampt);
            return INSTALL_ERROR;
        }
        if (pkg_pre_build_fingerprint.empty()) {
            LOGE("Downgrade package must have a pre-build version set, not "
                 "allowed.\n");
            return INSTALL_ERROR;
        }
    }

    return 0;
}

static int
update_binary_command(const char* path, ZipArchive* zip, int retry_count,
                      int status_fd, std::vector<std::string>* cmd)
{
    int ret = check_newer_ab_build(zip);
    if (ret) {
        return ret;
    }

    // For A/B updates we extract the payload properties to a buffer and obtain
    // the RAW payload offset in the zip file.
    const ZipEntry* properties_entry =
            mzFindZipEntry(zip, AB_OTA_PAYLOAD_PROPERTIES);
    if (!properties_entry) {
        LOGE("Can't find %s\n", AB_OTA_PAYLOAD_PROPERTIES);
        return INSTALL_CORRUPT;
    }
    std::vector<unsigned char> payload_properties(
            mzGetZipEntryUncompLen(properties_entry));
    if (!mzExtractZipEntryToBuffer(zip, properties_entry,
                                   payload_properties.data())) {
        LOGE("Can't extract %s\n", AB_OTA_PAYLOAD_PROPERTIES);
        return INSTALL_CORRUPT;
    }

    const ZipEntry* payload_entry = mzFindZipEntry(zip, AB_OTA_PAYLOAD);
    if (!payload_entry) {
        LOGE("Can't find %s\n", AB_OTA_PAYLOAD);
        return INSTALL_CORRUPT;
    }
    long payload_offset = mzGetZipEntryOffset(payload_entry);
    *cmd = {
        "/sbin/update_engine_sideload",
        android::base::StringPrintf("--payload=file://%s", path),
        android::base::StringPrintf("--offset=%ld", payload_offset),
        "--headers=" + std::string(payload_properties.begin(),
                                   payload_properties.end()),
        android::base::StringPrintf("--status_fd=%d", status_fd),
    };
    return 0;
}

#else  // !AB_OTA_UPDATER

static int
update_binary_command(const char* path, ZipArchive* zip, int retry_count,
                      int status_fd, std::vector<std::string>* cmd)
{
    char buf[PATH_MAX];
#ifdef TARGET_SUPPORTS_MPLANE_SPEC
    if(buildId == nullptr) {
        printf("update_binary_command: Not able to fetch build-id\n");
        return INSTALL_ERROR;
    }
    snprintf(buf, PATH_MAX, "build-id%s/%s", buildId, ASSUMED_UPDATE_BINARY_NAME);
#else
    memset(buf, '\0', sizeof(buf));
    snprintf(buf, PATH_MAX, "%s", ASSUMED_UPDATE_BINARY_NAME);
#endif

    printf("update-binary path %s \n",buf);

    // On traditional updates we extract the update binary from the package.
    const ZipEntry* binary_entry =
            mzFindZipEntry(zip, buf);
    if (binary_entry == NULL) {
        LOGE("File corrupted %s; can't find %s\n", path, buf);
        return INSTALL_CORRUPT;
    }

    const char* binary = "/tmp/update_binary";
    unlink(binary);
    int fd = creat(binary, 0755);
    if (fd < 0) {
        LOGE("Can't make %s\n", binary);
        return INSTALL_ERROR;
    }
    bool ok = false;
    if(update_binary_from_device) {
        printf("Using update-binary from device\n");
        int src_fd = open("/usr/bin/updater", O_RDONLY);
        if(src_fd < 0) {
            printf("can't open /usr/bin/updater\n");
            return INSTALL_ERROR;
        }
        char buffer[PATH_MAX];
        int err,n;
        while (1) {
            err = read(src_fd, buffer, PATH_MAX);
            if (err == -1) {
                printf("Error reading updater from the device.\n");
                close(src_fd);
                close(fd);
                return INSTALL_ERROR;
            }
            n = err;
            if (n == 0) {
                printf("Write done to %s\n", binary);
                break;
            }
            err = write(fd, buffer, n);
            if (err == -1) {
                printf("Error writing updater to %s \n", binary);
                close(src_fd);
                close(fd);
                return INSTALL_ERROR;
            }
        }
        close(src_fd);
    }
    else {
        printf("Using update-binary from package\n");
        ok = mzExtractZipEntryToFile(zip, binary_entry, fd);
    }
    close(fd);
    if (!ok && !update_binary_from_device) {
        LOGE("Can't copy %s\n", buf);
        return INSTALL_ERROR;
    }

#if defined(TARGET_SUPPORTS_MIRROR_AB_COPY) || defined(TARGET_SUPPORTS_MPLANE_SPEC)
    if(install_only) {
        LOGI("install only flow \n");
        *cmd = {
            binary,
            EXPAND(RECOVERY_API_VERSION),   // defined in Android.mk
            std::to_string(status_fd),
            path,
            "only_installation",
         };
    } else if(mirror_copy){
        LOGI("ab sync mirror flow \n");
        *cmd = {
            binary,
            EXPAND(RECOVERY_API_VERSION),   // defined in Android.mk
            std::to_string(status_fd),
            path,
            "copy_to_inactive",
        };
    }
    else {
        LOGI("update flow \n");
        *cmd = {
            binary,
            EXPAND(RECOVERY_API_VERSION),   // defined in Android.mk
            std::to_string(status_fd),
            path,
        };
    }
#else
     *cmd = {
         binary,
         EXPAND(RECOVERY_API_VERSION),   // defined in Android.mk
         std::to_string(status_fd),
         path,
     };
#endif

    if (retry_count > 0)
        cmd->push_back("retry");
    return 0;
}
#endif  // !AB_OTA_UPDATER

// If the package contains an update binary, extract it and run it.
static int
try_update_binary(const char* path, ZipArchive* zip, bool* wipe_cache,
                  std::vector<std::string>& log_buffer, int retry_count)
{
    read_source_target_build(zip, log_buffer);

    int pipefd[2];
    pipe(pipefd);

    std::vector<std::string> args;
    int ret = update_binary_command(path, zip, retry_count, pipefd[1], &args);
    mzCloseZipArchive(zip);
    if (ret) {
        close(pipefd[0]);
        close(pipefd[1]);
        return ret;
    }

    // When executing the update binary contained in the package, the
    // arguments passed are:
    //
    //   - the version number for this interface
    //
    //   - an fd to which the program can write in order to update the
    //     progress bar.  The program can write single-line commands:
    //
    //        progress <frac> <secs>
    //            fill up the next <frac> part of of the progress bar
    //            over <secs> seconds.  If <secs> is zero, use
    //            set_progress commands to manually control the
    //            progress of this segment of the bar.
    //
    //        set_progress <frac>
    //            <frac> should be between 0.0 and 1.0; sets the
    //            progress bar within the segment defined by the most
    //            recent progress command.
    //
    //        firmware <"hboot"|"radio"> <filename>
    //            arrange to install the contents of <filename> in the
    //            given partition on reboot.
    //
    //            (API v2: <filename> may start with "PACKAGE:" to
    //            indicate taking a file from the OTA package.)
    //
    //            (API v3: this command no longer exists.)
    //
    //        ui_print <string>
    //            display <string> on the screen.
    //
    //        wipe_cache
    //            a wipe of cache will be performed following a successful
    //            installation.
    //
    //        clear_display
    //            turn off the text display.
    //
    //        enable_reboot
    //            packages can explicitly request that they want the user
    //            to be able to reboot during installation (useful for
    //            debugging packages that don't exit).
    //
    //   - the name of the package zip file.
    //
    //   - an optional argument "retry" if this update is a retry of a failed
    //   update attempt.
    //

    // Convert the vector to a NULL-terminated char* array suitable for execv.
    const char* chr_args[args.size() + 1];
    chr_args[args.size()] = NULL;
    for (size_t i = 0; i < args.size(); i++) {
        chr_args[i] = args[i].c_str();
    }

    LOGI("Attempting to run %s\n", chr_args[0]);
    pid_t pid = fork();

    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        LOGE("Failed to fork update binary: %s\n", strerror(errno));
        return INSTALL_ERROR;
    }

    if (pid == 0) {
        umask(022);
        close(pipefd[0]);
        execv(chr_args[0], const_cast<char**>(chr_args));
        fprintf(stdout, "E:Can't run %s (%s)\n", chr_args[0], strerror(errno));
        LOGE("Can't run %s (%s)\n", chr_args[0], strerror(errno));
        _exit(-1);
    }
    close(pipefd[1]);

    *wipe_cache = false;
    bool retry_update = false;

    char buffer[1024];
    FILE* from_child = fdopen(pipefd[0], "r");
    while (fgets(buffer, sizeof(buffer), from_child) != NULL) {
        char* saveptr = NULL;
        char* command = strtok_r(buffer, " \n", &saveptr);
        if (command == NULL) {
            continue;
        } else if (strcmp(command, "progress") == 0) {
            char* fraction_s = strtok_r(NULL, " \n", &saveptr);
            char* seconds_s = strtok_r(NULL, " \n", &saveptr);

            float fraction = strtof(fraction_s, NULL);
            int seconds = strtol(seconds_s, NULL, 10);

            ui->ShowProgress(fraction * (1-VERIFICATION_PROGRESS_FRACTION), seconds);
        } else if (strcmp(command, "set_progress") == 0) {
            char* fraction_s = strtok_r(NULL, " \n", &saveptr);
            float fraction = strtof(fraction_s, NULL);
            ui->SetProgress(fraction);
        } else if (strcmp(command, "ui_print") == 0) {
            char* str = strtok_r(NULL, "\n", &saveptr);
            if (str) {
                ui->PrintOnScreenOnly("%s", str);
            } else {
                ui->PrintOnScreenOnly("\n");
            }
            fflush(stdout);
        } else if (strcmp(command, "wipe_cache") == 0) {
            *wipe_cache = true;
        } else if (strcmp(command, "clear_display") == 0) {
            ui->SetBackground(RecoveryUI::NONE);
        } else if (strcmp(command, "enable_reboot") == 0) {
            // packages can explicitly request that they want the user
            // to be able to reboot during installation (useful for
            // debugging packages that don't exit).
            ui->SetEnableReboot(true);
        } else if (strcmp(command, "retry_update") == 0) {
            retry_update = true;
        } else if (strcmp(command, "log") == 0) {
            // Save the logging request from updater and write to
            // last_install later.
            log_buffer.push_back(std::string(strtok_r(NULL, "\n", &saveptr)));
        } else {
            LOGE("unknown command [%s]\n", command);
        }
    }
    fclose(from_child);

    int status;
    waitpid(pid, &status, 0);
    if (retry_update) {
        return INSTALL_RETRY;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        LOGE("Error in %s\n(Status %d)\n", path, WEXITSTATUS(status));
        return INSTALL_ERROR;
    }

    return INSTALL_SUCCESS;
}

#ifdef USE_MDTP
static int
mdtp_update()
{
    const char** args = (const char**)malloc(sizeof(char*) * 2);

    if (args == NULL) {
        LOGE("Failed to allocate memory for MDTP FOTA app arguments\n");
        return 0;
    }

    args[0] = "/sbin/mdtp_fota";
    args[1] = NULL;
    int status = 0;

    ui->Print("Running MDTP integrity verification and update...\n");

    /* Make sure system partition is mounted, so MDTP can process its content. */
    status = mount("/dev/block/bootdevice/by-name/system", "/system", "ext4",
                 MS_NOATIME | MS_NODEV | MS_NODIRATIME |
                 MS_RDONLY, "");

    if (status) {
        LOGE("Failed to mount the system partition, error=%s.\n", strerror(errno));
        free(args);
        return 0;
    }

    status = mount("/dev/block/bootdevice/by-name/modem", "/firmware", "vfat",
                   MS_NOATIME | MS_NODEV | MS_NODIRATIME |
                   MS_RDONLY, "");

    if (status) {
        LOGE("Failed to mount the modem (firmware) partition, error=%s.\n", strerror(errno));
        free(args);
        return 0;
    }

    status = 0;

    pid_t pid = fork();
    if (pid == 0) {
        execv(args[0], (char* const*)args);
        LOGE("Can't run %s (%s)\n", args[0], strerror(errno));
        _exit(-1);
    }
    if (pid > 0) {
        LOGE("Waiting for MDTP FOTA to complete...\n");
        pid = waitpid(pid, &status, 0);
        LOGE("MDTP FOTA completed, status: %d\n", status);
    }

    /* Leave the system partition unmounted before we finish. */
    umount("/system");
    umount("/firmware");

    free(args);

    return (status > 0) ? 1 : 0;
}
#endif /* USE_MDTP */

#ifdef TARGET_SUPPORTS_MPLANE_SPEC
int parse_file(xmlNode *objNode,ZipArchive* zip)
{
    int ret = 1;
    xmlChar *name;
    xmlChar *path;

    name = xmlGetProp(objNode, BAD_CAST "fileName");
    if(!name) {
        printf("File name value is missing \n");
        return 1;
    }
    else
        printf("File name: %s \n", name);
    if(!xmlStrncmp(name, BAD_CAST "system.img", xmlUTF8Size(name))){
        printf("Skipping for system image\n");
        return 0;
    }
    char buffer[PATH_MAX];
    if(buildId == nullptr) {
        printf("parse_file: Not able to fetch build-id\n");
        return 1;
    }
    if(!xmlStrncmp(name, BAD_CAST "boot.img", xmlUTF8Size(name))){
        snprintf(buffer, PATH_MAX, "build-id%s/%s", buildId, (char *) name);
        const ZipEntry* find_entry_inside_zip = mzFindZipEntry(zip, buffer);
        if (find_entry_inside_zip == NULL) {
            printf("failed to find %s in ZipArchive\n", (char *) name);
            return 1;
        }
        return 0;
    }
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, PATH_MAX, "build-id%s/firmware-update/%s", buildId, (char *) name);
    const ZipEntry* find_entry_inside_zip = mzFindZipEntry(zip, buffer);
    if (find_entry_inside_zip == NULL) {
        printf("failed to find %s in ZipArchive\n", buffer);
        return 1;
    }

    return 0;
}


xmlChar *get_build_id_from_build_node(xmlNode *objNode, ZipArchive* zip)
{
    xmlChar *id;
    id = xmlGetProp(objNode, BAD_CAST "id");
    if(!id)
        printf("Build ID value is missing \n");
    else
        printf("Build ID: %s \n", id);
    return id;
}

xmlChar *get_build_id_from_product_node(xmlNode *objNode)
{
    xmlChar *build_Id;
    build_Id = xmlGetProp(objNode, BAD_CAST "build-Id");
    if(!build_Id)
        printf("Product buildId value is missing \n");
    else
        printf("Product buildId: %s \n", build_Id);
   return build_Id;
}
int compare_manifest_images_with_update_package_images(xmlNode *objNode,ZipArchive* zip){
    int ret = 1;
    xmlNode *filenode = NULL;
    for(filenode = objNode->children; filenode; filenode = filenode->next)
    {
        if (filenode->type != XML_ELEMENT_NODE) {
            continue;
        }
        if (strncmp((const char *)filenode->name, "file", strlen("file")) == 0) {
            ret = parse_file(filenode, zip);
            if(ret != 0)
                return ret;
        }
    }
    return ret;
}

int validate_manifest(ZipArchive* zip)
{
    int product_flag = 0, build_flag = 0, ret = 1;
    xmlDocPtr doc;
    xmlNodePtr rootNode = NULL;
    xmlNodePtr objNode, childNode;
    char *name = NULL;
    char *value = NULL;
    char *build_id_in_product_node = NULL;
    char *build_id_in_build_node = NULL;
    buildId = get_build_id_from_manifest(zip);
    char buf[PATH_MAX];
    if(buildId == nullptr) {
        printf("validate_manifest: Not able to fetch build-id\n");
        return 1;
    }
    snprintf(buf, PATH_MAX, "build-id%s/%s", buildId, MANIFEST_FILE_NAME);
    const ZipEntry* manifest_entry = mzFindZipEntry(zip, buf);
    if (manifest_entry == NULL) {
        printf("failed to find %s\n", buf);
        return 1;
    }
    const char* manifest = MANIFEST_FILE_PATH;
    unlink(manifest);
    int manifest_fd = creat(manifest, 0644);
    if (manifest_fd < 0) {
        printf("%s: Can't make\n", manifest);
        return 1;
    }
    bool ok = mzExtractZipEntryToFile(zip, manifest_entry, manifest_fd);
    close(manifest_fd);
    if (!ok) {
        printf("%s: Can't extract from zip\n", MANIFEST_FILE_PATH);
        return 1;
    }
    printf("start to load xml: %s", MANIFEST_FILE_PATH);

    doc = xmlReadFile(MANIFEST_FILE_PATH, "UTF-8", XML_PARSE_RECOVER);
    if(NULL == doc) {
        printf("XML Document not parsed successfully.\n ");
        return 1;
    }

    rootNode = xmlDocGetRootElement(doc);
    if(NULL == rootNode) {
        printf("Empty XML document \n");
        xmlFreeDoc(doc);
        return 1;
    }

    for(objNode = rootNode->children; objNode; objNode = objNode->next) {
        if (objNode->type != XML_ELEMENT_NODE) {
            continue;
        }

        printf("Parsing element , line %u: name=%s \n", objNode->line, (char*)objNode->name);

        if (strncmp((const char *)objNode->name, "products", strlen("products")) == 0) {
            for(childNode = objNode->children; childNode; childNode = childNode->next) {
                if (childNode->type != XML_ELEMENT_NODE)
                    continue;
                printf("Parsing element , line %u: name=%s \n",childNode->line, (char*)childNode->name);
                if (!product_flag && !strncmp((const char *)childNode->name, "product", strlen("product")))
                {
                    product_flag = 1;
                    build_id_in_product_node = (char *) get_build_id_from_product_node(childNode);
                }
            }
        } else if (strncmp((const char *)objNode->name, "builds", strlen("builds")) == 0) {
            for(childNode = objNode->children; childNode; childNode = childNode->next) {
                if (childNode->type != XML_ELEMENT_NODE)
                        continue;
                printf("Parsing element , line %u: name=%s \n",childNode->line, (char*)childNode->name);
                if (!build_flag && !strncmp((const char *)childNode->name, "build", strlen("build"))) {
                    build_flag = 1;
                    build_id_in_build_node = (char *) get_build_id_from_build_node(childNode,zip);
                    if(build_id_in_product_node != NULL && build_id_in_build_node != NULL \
                           && strncmp(build_id_in_product_node, build_id_in_build_node, strlen(build_id_in_build_node)) == 0) {
                        printf("build_id_in_product_node and build_id_in_build_node is same\n");
                        ret = compare_manifest_images_with_update_package_images(childNode,zip);
                        if(ret == 0){
                            printf("All the images are present inside zip\n");
                            return 0;
                        }
                    }
                }
            }
        }
    }
    return ret;
}
#endif
static int
really_install_package(const char *path, bool* wipe_cache, bool needs_mount,
                       std::vector<std::string>& log_buffer, int retry_count)
{
    LOGI("really_install_package\n");
    ui->SetBackground(RecoveryUI::INSTALLING_UPDATE);
    ui->Print("Finding update package...\n");
    // Give verification half the progress bar...
    ui->SetProgressType(RecoveryUI::DETERMINATE);
    ui->ShowProgress(VERIFICATION_PROGRESS_FRACTION, VERIFICATION_PROGRESS_TIME);
    LOGI("Update location: %s\n", path);

    // Map the update package into memory.
    ui->Print("Opening update package...\n");

    if (path && needs_mount) {
        if (path[0] == '@') {
            ensure_path_mounted(path+1);
        } else {
            ensure_path_mounted(path);
        }
    }

    MemMapping map;
    if (sysMapFile(path, &map) != 0) {
        LOGE("failed to map file\n");
        return INSTALL_CORRUPT;
    }

    // Try to open the package.
    ZipArchive zip = {0};
    int err = mzOpenZipArchive(map.addr, map.length, &zip);
    if (err != 0) {
        LOGE("Can't open %s\n(%s)\n", path, err != -1 ? strerror(err) : "bad");
        log_buffer.push_back(android::base::StringPrintf("error: %d", kZipOpenFailure));
        sysReleaseMap(&map);
        return INSTALL_CORRUPT;
    }

#ifdef TARGET_SUPPORTS_OTA_VERIFICATION
    // Verify package.
    if (!verify_ota_package(path, &zip)) {
        log_buffer.push_back(android::base::StringPrintf("error: %d", kZipVerificationFailure));
        sysReleaseMap(&map);
        return INSTALL_CORRUPT;
    }
#endif
#ifdef TARGET_SUPPORTS_MPLANE_SPEC
    if(validate_manifest(&zip)){
        printf("Manifest entries or build-id is not correct\n");
        sysReleaseMap(&map);
        return INSTALL_CORRUPT;
    }
#endif
    // Verify and install the contents of the package.
    ui->Print("Installing update...\n");
    LOGI("Installing update...\n");
    if (retry_count > 0) {
        ui->Print("Retry attempt: %d\n", retry_count);
    }
    ui->SetEnableReboot(false);
    int result = try_update_binary(path, &zip, wipe_cache, log_buffer, retry_count);
    LOGI(" enum INSTALL_SUCCESS: %d , and update result:  %d \n",INSTALL_SUCCESS,result);
    ui->SetEnableReboot(true);
    ui->Print("\n");

    sysReleaseMap(&map);

#ifdef USE_MDTP
    /* If MDTP update failed, return an error such that recovery will not finish. */
    if (result == INSTALL_SUCCESS) {
        if (!mdtp_update()) {
            ui->Print("Unable to verify integrity of /system for MDTP, update aborted.\n");
            return INSTALL_ERROR;
        }
        ui->Print("Successfully verified integrity of /system for MDTP.\n");
    }
#endif /* USE_MDTP */

    return result;
}

static bool startswith(const char *string, const char *prefix)
{
       if(string != NULL && prefix != NULL){
           size_t l1 = strlen(string);
           size_t l2 = strlen(prefix);
           return strncmp(string, prefix, MIN(l1, l2)) == 0;
       }
       return false;
}

int
install_package(const char* path, bool* wipe_cache, const char* install_file,
                bool needs_mount, int retry_count)
{

    LOGI(" install_package\n");
    modified_flash = true;
    auto start = std::chrono::system_clock::now();

    int result = 0;
    std::vector<std::string> log_buffer;
    if (needs_mount == true)
            result = setup_install_mounts();
    if (result != 0) {
        LOGE("failed to set up expected mounts for install; aborting\n");
        result = INSTALL_ERROR;
    } else {
        result = really_install_package(path, wipe_cache, needs_mount, log_buffer, retry_count);
    }

    // Measure the time spent to apply OTA update in seconds.
    std::chrono::duration<double> duration = std::chrono::system_clock::now() - start;
    int time_total = static_cast<int>(duration.count());

    if (ensure_path_mounted(UNCRYPT_STATUS) != 0) {
        LOGW("Can't mount %s\n", UNCRYPT_STATUS);
    } else {
        std::string uncrypt_status;
        if (!android::base::ReadFileToString(UNCRYPT_STATUS, &uncrypt_status)) {
            LOGW("failed to read uncrypt status: %s\n", strerror(errno));
        } else if (!android::base::StartsWith(uncrypt_status, "uncrypt_")) {
            LOGW("corrupted uncrypt_status: %s: %s\n", uncrypt_status.c_str(), strerror(errno));
        } else {
            log_buffer.push_back(android::base::Trim(uncrypt_status));
        }
    }

    // The first two lines need to be the package name and install result.
    std::vector<std::string> log_header = {
        path,
        result == INSTALL_SUCCESS ? "1" : "0",
        "time_total: " + std::to_string(time_total),
        "retry: " + std::to_string(retry_count),
    };
#ifdef USE_LE_MODE
    // TODO : Check if any issue with using character join
    std::string log_content = android::base::Join(log_header, '\n') + "\n" +
            android::base::Join(log_buffer, '\n');
#else
    std::string log_content = android::base::Join(log_header, "\n") + "\n" +
            android::base::Join(log_buffer, "\n");
#endif
    if (!android::base::WriteStringToFile(log_content, install_file)) {
        LOGE("failed to write %s: %s\n", install_file, strerror(errno));
    }
#ifdef TARGET_SUPPORTS_MPLANE_SPEC
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    FILE *fp;
    int cause_code=-1,error_code=-1;
    fp = fopen(install_file, "r");
    if (!fp)
        printf("Couldn't open install file\n");

    while ((read = getline(&line, &len, fp)) != -1) {
        if (startswith(line, "cause:")) {
            sscanf(line, "cause: %d\n", &cause_code);
            continue;
        }
        if (startswith(line, "error:")) {
            sscanf(line, "error: %d\n", &error_code);
            continue;
        }
    }
    
    printf("cause code: %d error_code %d\n",cause_code, error_code);
    //these error codes are from causecode enum in error_code.h
    if((cause_code =! -1 && cause_code>=100 && cause_code<=113))
        result = INSTALL_ERROR;
#endif
    // Write a copy into last_log.
    LOGI("%s\n", log_content.c_str());

    // regardless of whether OTA-upgrade failed or not,
    // clear any existing cookie.
    int err = 0, ret = unlink(RECOVERYUPDATER_COOKIE); // remove any existing cookie
    err = errno;

    if (ret == 0) {
        LOGE("\nRemoved existing cookie!! \n");
    } else {
        LOGE("\nFailure in removing cookie, %s\n", strerror(err));
    }

    return result;
}

bool verify_package(const unsigned char* package_data, size_t package_size) {
#ifndef USE_LE_MODE
    std::vector<Certificate> loadedKeys;
    if (!load_keys(PUBLIC_KEYS_FILE, loadedKeys)) {
        LOGE("Failed to load keys\n");
        return false;
    }
    LOGI("%zu key(s) loaded from %s\n", loadedKeys.size(), PUBLIC_KEYS_FILE);

    // Verify package.
    ui->Print("Verifying update package...\n");
    LOGI("Verifying update package...\n");
    auto t0 = std::chrono::system_clock::now();
    int err = verify_file(const_cast<unsigned char*>(package_data), package_size, loadedKeys);
    std::chrono::duration<double> duration = std::chrono::system_clock::now() - t0;
    ui->Print("Update package verification took %.1f s (result %d).\n", duration.count(), err);
    if (err != VERIFY_SUCCESS) {
        LOGE("Signature verification failed\n");
        LOGE("error: %d\n", kZipVerificationFailure);
        return false;
    }
    return true;
#else
    return false;
#endif
}

#ifdef TARGET_SUPPORTS_OTA_VERIFICATION
bool verify_ota_package(const char* path, ZipArchive *zip) {

    bool result = false;

    //Extract signature file from the zip
    const ZipEntry* sig_entry =
            mzFindZipEntry(zip, SIGNATURE_FILE_NAME);
    if (sig_entry == NULL) {
        LOGE("can't find %s\n", sig_entry);
        return result;
    }
    const char* sig_file = SIGNATURE_FILE;
    unlink(sig_file);
    int fd = creat(sig_file, 0644);
    if (fd < 0) {
        LOGE("Can't make %s\n", sig_file);
        return result;
    }
    bool ok = mzExtractZipEntryToFile(zip, sig_entry, fd);
    close(fd);
    if (!ok) {
        LOGE("Can't extract %s from zip\n", SIGNATURE_FILE_NAME);
        return result;
    }

    //Verify the signature
    FILE *fp;
    char cmd_buf[CMD_BUFFER_SIZE] = { 0 };

    snprintf(cmd_buf, sizeof(cmd_buf), "unzip -p %s -x %s | openssl dgst -sha256 -verify %s -signature %s 2>&1", path, SIGNATURE_FILE_NAME, PUBLIC_KEY, SIGNATURE_FILE);
    fp = popen(cmd_buf, "r");
    if(!fp) {
        LOGE("Can't run openssl command\n");
        return false;
    }
    char buf[CMD_BUFFER_SIZE] = { 0 };
    std::string openssl_result = "";
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        openssl_result += buf;
    }
    LOGI("openssl command result = %s", openssl_result.c_str());
    if (!strncmp(openssl_result.c_str(), OTA_VERIFICATION_SUCCESS, strlen(OTA_VERIFICATION_SUCCESS))) {
        result = true;
        LOGI("OTA package verification successful\n");
    } else {
        LOGI("OTA package verification failed\n");
    }
    pclose(fp);
    unlink(sig_file);

    return result;
}
#endif //TARGET_SUPPORTS_OTA_VERIFICATION
