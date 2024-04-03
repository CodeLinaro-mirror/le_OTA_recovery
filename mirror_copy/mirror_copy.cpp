/**********************************************************
Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause-Clear
*********************************************************/

#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <log/logger.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "common.h"

#ifdef TARGET_SUPPORTS_AB
#ifndef TARGET_NAD_OTA
#include <libabctl.h>
#else
#include <nad-ab-al.h>
#endif
#endif
static const char *MIRROR_COPY_STATUS_COOKIE_FILE = "/cache/recovery/mirror_copy_status";
static const char *TEMPORARY_LOG_FILE = "/cache/recovery/mirror_copy.log";
static const char *ZIP_FILE_PATH = "/cache/recovery/update_package_path";
static const int MAX_ARG_LENGTH = 4096;

void tokenize(std::string const &str, char delim,
              std::vector<std::string> &out) {
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, delim)) {
        out.push_back(std::string(token));
    }
}

bool is_file_empty(std::fstream &pFile) {
    bool result = (pFile.peek() == std::fstream::traits_type::eof());
    pFile.seekg(0, std::ios::beg);
    return result;
}

std::string read_first_line(std::fstream &newfile) {
    if (newfile.is_open()) {
        std::string tp;
        while (getline(newfile, tp)) {
            return tp;
        }
    }
    return "";
}

std::string read_last_line(std::fstream &fin) {
    if (fin.is_open()) {
        fin.seekg(-1, std::ios_base::end);  // go to one spot before the EOF
        char ch;
        fin.get(ch);
        if (ch == '\n') fin.seekg(-2, std::ios_base::end);
        bool keepLooping = true;
        while (keepLooping) {
            char ch;
            fin.get(ch);  // Get current byte's data

            if ((int)fin.tellg() <=
                1) {           // If the data was at or before the 0th byte
                fin.seekg(0);  // The first line is the last line
                keepLooping = false;  // So stop there
            } else if (ch == '\n') {  // If the data was a newline
                keepLooping = false;  // Stop at the current position.
            } else {  // If the data was neither a newline nor at the 0 byte
                fin.seekg(-2, std::ios_base::cur);  // Move to the front of that
                                                    // data, then to the front
                                                    // of the data before it
            }
        }
        std::string lastLine;
        getline(fin, lastLine);                       // Read the current line
        std::cout << "Result: " << lastLine << '\n';  // Display it
        return lastLine;
    }
    return "";
}




int main() {
    FILE *stream = freopen(TEMPORARY_LOG_FILE, "a", stdout);
    if(stream == NULL){
        std::cout << "Temp Log file re-open failed\n";
        return 0;
    }

    std::cout << "Mirror Copy Started\n";
    int boot_slot = -1, ota_started_slot = -1, boot_slot_from_ota_status = -1,
        boot_success_status = -1, retry_count = 0, MAX_RETRY_COUNT = 5;
    while(boot_success_status != 1 && retry_count < MAX_RETRY_COUNT) {
#ifdef TARGET_SUPPORTS_AB
        boot_slot = libabctl_getBootSlot();
        boot_success_status = libabctl_getSuccessStatus(boot_slot);
        if(boot_success_status == 1)
            break;
#endif
        sleep(60);
        retry_count++;
    }
    if(boot_success_status != 1) {
        std::cout << "Success bit is not set so exiting\n";
        return 0;
    }
    std::fstream file;
    file.open(MIRROR_COPY_STATUS_COOKIE_FILE,
              std::ios::in | std::ios::out | std::ios::app);
    if (!file) {
        std::cout << "File is not existed\n";
        file.close();
        return 0;
    }
    if (is_file_empty(file)) {
        std::cout << "File is empty\n";
        file.close();
        return 0;
    } else {
        std::string stage, ota_status, slot_from_ota_status, first_line,
            last_line, last_line_new, ota_started_slot_str, ota_status_new,
            slot_from_ota_status_new;
        char delim = ':';
        char zip_path[MAX_ARG_LENGTH];
        FILE *fp;
        fp = fopen(ZIP_FILE_PATH, "r");
        if (fp == NULL) {
            std::cout << "Zip file open failed\n";
            return 0;
        }
        else
        {
            fscanf(fp, "%s\n", zip_path);
            fclose(fp);
        }
        std::vector<std::string> first_line_split;
        std::vector<std::string> last_line_split;
        std::vector<std::string> last_line_split_new;
        first_line = read_first_line(file);
        last_line = read_last_line(file);
        tokenize(last_line, delim, last_line_split);
        tokenize(first_line, delim, first_line_split);
        if (first_line_split.size() != 3 || last_line_split.size() != 3) {
            std::cout << "Invalid Entries in the cookie file\n";
            return 0;
        }
        stage = last_line_split[0];
        ota_status = last_line_split[1];
        slot_from_ota_status = last_line_split[2];
        boot_slot_from_ota_status = slot_from_ota_status[0] - '0';
        ota_started_slot_str = first_line_split[2];
        ota_started_slot = ota_started_slot_str[0] - '0';
        file.close();
        std::cout << "current boot slot" << boot_slot << "\n";
        std::cout << "stage: " << stage << " ota_status: " << ota_status
                  << " zip_path: " << zip_path << "\n";


        // triggering the mirror copy
        if (stage == "STAGE1" && ota_status == "OTA_COMPLETED" &&
                 boot_slot == boot_slot_from_ota_status &&
                 (ota_started_slot + 1) % 2 == boot_slot) {
            std::cout << "triggering the mirror copy\n";
            fclose(stream);
            char *args[] = {"/usr/bin/recovery", zip_path, NULL};
            execv(args[0], args);
        }

        // triggering the mirror copy again in recovery
        else if (stage == "STAGE2" && ota_status == "COPY_STARTED" &&
                 (ota_started_slot + 1) % 2 == boot_slot) {
            std::cout << "triggering the mirror copy again\n";
            fclose(stream);
            char *args[] = {"/usr/bin/recovery", zip_path, NULL};
            execv(args[0], args);
            file.open(MIRROR_COPY_STATUS_COOKIE_FILE,
                 std::ios::in);
            if (is_file_empty(file)) {
                std::cout << "File is empty\n";
                file.close();
                return 0;
            }
            last_line_new = read_last_line(file);
            tokenize(last_line_new, delim, last_line_split_new);
            if (last_line_split_new.size() != 3) {
                std::cout << "Invalid Entries in the cookie file\n";
                return 0;
            }
            ota_status_new = last_line_split_new[1];
            if(ota_status_new == "COPY_COMPLETED") {
                std::string line;
                while (getline(file, line)) {
                    std::cout << line << "\n";
                }
                file.close();
                file.open(MIRROR_COPY_STATUS_COOKIE_FILE,
                       std::ofstream::out | std::ofstream::trunc);
                file.close();
            }
        }
    }
    file.close();
    fclose(stream);

    return 0;
}
