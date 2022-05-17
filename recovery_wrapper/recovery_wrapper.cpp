/**********************************************************
Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause-Clear
*********************************************************/

#include <getopt.h>
#include <unistd.h>

#include <iostream>
#ifdef TARGET_SUPPORTS_AB
#include <libabctl.h>
#endif
using namespace std;

static const char *TEMPORARY_LOG_FILE = "/cache/recovery/update_engine.log";
static const struct option OPTIONS[] = {
    {"boot_successful", no_argument, NULL, 'b'},
    {"ota_update", no_argument, NULL, 'o'},
    {NULL, 0, NULL, 0},
};

int main(int argc, char **argv) {
    bool boot_successful = false;
    bool ota_update = false;
    int arg, option_index;
    while ((arg = getopt_long(argc, argv, "", OPTIONS, &option_index)) != -1) {
        switch (arg) {
            case 'b':
                boot_successful = true;
                break;
            case 'o':
                ota_update = true;
                break;
        }
    }
    if (ota_update) {
        if(remove(TEMPORARY_LOG_FILE) == 0) {
            std::cout << "Removed update_engine logs file\n";
        }
        char *args[] = {"/usr/bin/recovery",
                        "--update_package=/data/update_ext4.zip", NULL};
        execv(args[0], args);
        std::cout << "Received ota_update\n";
    }
    if (boot_successful) {
        cout << "Received boot_successful\n";
        int ret = libabctl_SetBootSuccess();
        if (ret == 0) {
            std::cout << "Marked bootsuccessful\n";
        } else {
            std::cout << "Booting failed from the given slot\n";
        }
    }
}

