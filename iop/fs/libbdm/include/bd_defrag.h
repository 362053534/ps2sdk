/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

/**
 * @file
 * Fragmented Block Device to Block Device
 */

#ifndef __BDM_DEFRAG_H__
#define __BDM_DEFRAG_H__


#include <tamtypes.h>
#include <bdm.h>
#include <usbhdfsd-common.h>

typedef struct bd_defrag_cursor {
    struct block_device *bd;
    struct bd_fragment *fraglist;
    u32 fragcount;
    u32 fragment;
    u64 offset;
} bd_defrag_cursor_t;

typedef struct bd_defrag_checkpoint {
    u64 logical_sector;
    u32 fragment;
} __attribute__((packed)) bd_defrag_checkpoint_t;

typedef struct bd_defrag_index {
    struct bd_fragment *fraglist;
    u32 fragcount;
    u32 stride;
    u32 checkpoint_count;
    bd_defrag_checkpoint_t *checkpoints;
} bd_defrag_index_t;

/* 游标由调用方持有；首次使用以及设备或碎片表变化后必须重置。 */
extern void bd_defrag_cursor_reset(bd_defrag_cursor_t *cursor);
extern int bd_defrag_index_build(bd_defrag_index_t *index, struct bd_fragment *fraglist, u32 fragcount,
                                 u32 stride, bd_defrag_checkpoint_t *checkpoints, u32 checkpoint_capacity);
extern void bd_defrag_index_reset(bd_defrag_index_t *index);
extern int bd_defrag_read_cached_indexed(struct block_device *bd, u32 fragcount, struct bd_fragment *fraglist,
                                         const bd_defrag_index_t *index, u64 sector, void *buffer, u16 count,
                                         bd_defrag_cursor_t *cursor);
extern int bd_defrag_read_cached(struct block_device *bd, u32 fragcount, struct bd_fragment *fraglist,
                                 u64 sector, void *buffer, u16 count, bd_defrag_cursor_t *cursor);
extern int bd_defrag_read(struct block_device* bd, u32 fragcount, struct bd_fragment* fraglist, u64 sector, void* buffer, u16 count);
extern int bd_defrag_write(struct block_device* bd, u32 fragcount, struct bd_fragment* fraglist, u64 sector, const void* buffer, u16 count);

// For backwards compatibility:
#define bd_defrag bd_defrag_read

#endif
