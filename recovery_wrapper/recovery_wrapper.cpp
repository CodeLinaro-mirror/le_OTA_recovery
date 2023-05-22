/**********************************************************
Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause-Clear
*********************************************************/

#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

#include <iostream>
#ifdef TARGET_SUPPORTS_AB
#ifndef TARGET_NAD_OTA
#include <libabctl.h>
#else
#include <nad-ab-al.h>
#endif
#endif
using namespace std;

static const char *TEMPORARY_LOG_FILE = "/cache/recovery/update_engine.log";
static const char *ZIP_FILE_PATH = "/cache/recovery/update_package_path";
static const int MAX_ARG_LENGTH = 4096;
static const struct option OPTIONS[] = {
    {"boot_successful", no_argument, NULL, 'b'},
    {"update_package", required_argument, NULL, 'o'},
    {NULL, 0, NULL, 0},
};

int main(int argc, char **argv) {
    bool boot_successful = false;
    const char *update_package = NULL;
    int arg, option_index;
    while ((arg = getopt_long(argc, argv, "", OPTIONS, &option_index)) != -1) {
        switch (arg) {
            case 'b':
                boot_successful = true;
                break;
            case 'o':
                update_package = optarg;
                break;
        }
    }
    if (update_package != NULL) {
        if (remove(TEMPORARY_LOG_FILE) == 0) {
            std::cout << "Removed update_engine logs file\n";
        }
        char zip_path[MAX_ARG_LENGTH];
        snprintf(zip_path, MAX_ARG_LENGTH, "--update_package=%s", update_package);
        std::cout << "Received ota_update and update_package path: " << zip_path
                  << endl;
        FILE *fp;
        fp = fopen(ZIP_FILE_PATH, "w");
        if (fp == NULL) {
            std::cout << "Zip file open failed\n";
            return 0;
        }
        else
        {
            fprintf(fp, "%s\n", zip_path);
            fclose(fp);
        }
        char *args[] = {"/usr/bin/recovery", zip_path, NULL};
        execv(args[0], args);
    }
    if (boot_successful) {
        cout << "Received boot_successful\n";
        int ret=-1;
#ifdef TARGET_SUPPORTS_AB
#ifndef TARGET_NAD_OTA
        ret = libabctl_SetBootSuccess();
#endif
#endif
        if (ret == 0) {
            std::cout << "Marked bootsuccessful\n";
        } else {
            std::cout << "Booting failed from the given slot\n";
        }
    }
    return 0;
}
