/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 * Not a contribution.
 *
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

#include "copyrawpart.h"
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <cstddef>
#include <limits.h>
#include <glib.h>

#define RAW_PART_LENGTH 20

static const char* slot_suffix_arr[] = {"_a", "_b", NULL};

int mrc_systems_raw_ab_sync (int src_slot)
{
    int destn_slot = (src_slot+1)%2;
    mtd_scan_partitions();
    destn_slot = (src_slot+1)%2;

    const MtdState *mtd_state = mtd_get_mtdstate();
    if (mtd_state == NULL) {
        printf(" mtdstate is not initialized ");
        return -1;
    }

    int i;
    MtdPartition *p;
    char src_raw_partition[RAW_PART_LENGTH];
    char destn_raw_partition[RAW_PART_LENGTH];
    char buf[RAW_PART_LENGTH];
    char src_mtd_block[PATH_MAX];
    // start from tz and  write till boot ab raw partitions
    for (i = 0; i < mtd_state->partitions_allocd; i++)
    {
        p = &mtd_state->partitions[i];
        if (p->device_index >= 0 && p->name != NULL) {
            memset(src_raw_partition, 0, RAW_PART_LENGTH);
            memset(destn_raw_partition, 0, RAW_PART_LENGTH);
            memset(buf, 0, RAW_PART_LENGTH);
            memset(src_mtd_block, 0, PATH_MAX);
            strlcat(buf, p->name, strlen(p->name) - 1);
            if(strlen(buf)==0)
            {
                printf("partname is empty \n");
                continue;
            }
            snprintf(src_raw_partition, strlen(buf)+3, "%s%s", buf,
                slot_suffix_arr[src_slot]);
            snprintf(destn_raw_partition, strlen(buf)+3, "%s%s", buf,
                slot_suffix_arr[destn_slot]);

            printf("copying %s to %s\n", src_raw_partition, destn_raw_partition);

            const MtdPartition* src_mtd = mtd_find_partition_by_name(src_raw_partition);
            if(src_mtd == NULL) {
                printf("failed to mtd of partition :%s \n",src_raw_partition);
                return -1;
            }
            snprintf(src_mtd_block, sizeof(src_mtd_block), "/dev/mtdblock%d", src_mtd->device_index);
 
            printf("\n source mtd block  %s \n",src_mtd_block);

            const MtdPartition* mtd;
            char* result = NULL;
            mtd = mtd_find_partition_by_name(destn_raw_partition);
            if (mtd == NULL) {
                printf("no mtd partition named \"%s\"\n", destn_raw_partition);
                return -1;
            }

            MtdWriteContext* ctx;
            ctx = mtd_write_partition(mtd);
            if (ctx == NULL) {
                printf("can't write mtd partition \"%s\"\n", destn_raw_partition);
                return -1;
            }

             bool success;
             char* filename = src_mtd_block;
             FILE* f = fopen(filename, "rb");
             if (f == NULL) {
                 printf(" can't open %s: %s\n",  filename, strerror(errno));
                 return -1;
             }

             success = true;
             char* buffer = reinterpret_cast<char*>(malloc(BUFSIZ));
             int read;
             while (success && (read = fread(buffer, 1, BUFSIZ, f)) > 0) {
                 int wrote = mtd_write_data(ctx, buffer, read);
                 success = success && (wrote == read);
             }
             free(buffer);
             fclose(f);

             if (!success) {
                 printf("mtd_write_data to %s failed: %s\n",
                 destn_raw_partition, strerror(errno));
             }
             // this code seems to be erasing partition but after dump data written to partition is proper
             if (mtd_erase_blocks(ctx, -1) == -1) {
                 printf("error erasing blocks of %s\n", destn_raw_partition);
             }
             if (mtd_write_close(ctx) != 0) {
                 printf("error closing write of %s\n", destn_raw_partition);
             }
        }
    }
    return 0;
}
