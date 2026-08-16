#include <bd_defrag.h>

//#define DEBUG  //comment out this line when not debugging
#include "module_debug.h"


void bd_defrag_cursor_reset(bd_defrag_cursor_t *cursor)
{
    if (cursor != NULL) {
        cursor->bd = NULL;
        cursor->fraglist = NULL;
        cursor->fragcount = 0;
        cursor->fragment = 0;
        cursor->offset = 0;
    }
}

static void bd_defrag_cursor_set(bd_defrag_cursor_t *cursor, struct block_device *bd, u32 fragcount, struct bd_fragment *fraglist, u32 fragment, u64 offset)
{
    if (cursor != NULL) {
        cursor->bd = bd;
        cursor->fraglist = fraglist;
        cursor->fragcount = fragcount;
        cursor->fragment = fragment;
        cursor->offset = offset;
    }
}

static int bd_defrag_locate(struct block_device *bd, u32 fragcount, struct bd_fragment *fraglist, u64 sector, u32 *fragment, u64 *offset, bd_defrag_cursor_t *cursor)
{
    u32 i = 0;
    u64 current_offset = 0;

    /* 顺序读取时从上次命中的碎片继续查找，随机回跳时退回表头。 */
    if (cursor != NULL && cursor->bd == bd && cursor->fraglist == fraglist && cursor->fragcount == fragcount && cursor->fragment < fragcount) {
        struct bd_fragment *cached = &fraglist[cursor->fragment];

        if (cursor->offset <= sector) {
            i = cursor->fragment;
            current_offset = cursor->offset;
            if (sector < current_offset + cached->count) {
                *fragment = i;
                *offset = current_offset;
                return 0;
            }

            current_offset += cached->count;
            i++;
        }
    }

    for (; i < fragcount; i++) {
        struct bd_fragment *f = &fraglist[i];

        if (current_offset <= sector && sector < current_offset + f->count) {
            *fragment = i;
            *offset = current_offset;
            return 0;
        }
        current_offset += f->count;
    }

    bd_defrag_cursor_reset(cursor);
    return -1;
}

int bd_defrag_read_cached(struct block_device *bd, u32 fragcount, struct bd_fragment *fraglist, u64 sector, void *buffer, u16 count, bd_defrag_cursor_t *cursor)
{
    bd_defrag_cursor_t local_cursor;
    u64 sector_start = sector;
    u16 count_left = count;

    if (count == 0)
        return 0;

    /* 连续文件保持一次定位、一次底层读取，不进入多碎片路径。 */
    if (fragcount == 1 && sector < fraglist[0].count && count <= fraglist[0].count - sector) {
        bd_defrag_cursor_set(cursor, bd, fragcount, fraglist, 0, 0);
        if (bd->read(bd, fraglist[0].sector + sector, buffer, count) != count) {
            M_PRINTF("%s: ERROR: read failed!\n", __FUNCTION__);
            return -1;
        }
        return count;
    }

    if (cursor == NULL) {
        bd_defrag_cursor_reset(&local_cursor);
        cursor = &local_cursor;
    }

    while (count_left > 0) {
        u16 count_read;
        u64 offset;
        u64 readable;
        u64 physical_end;
        struct bd_fragment *f;
        u32 fragment;
        u32 last_fragment;
        u64 last_offset;

        if (bd_defrag_locate(bd, fragcount, fraglist, sector_start, &fragment, &offset, cursor) != 0) {
            M_PRINTF("%s: ERROR: fragment not found!\n", __FUNCTION__);
            return -1;
        }

        f = &fraglist[fragment];
        readable = offset + f->count - sector_start;
        physical_end = f->sector + f->count;
        last_fragment = fragment;
        last_offset = offset;

        /* 物理地址连续的相邻项可以合成同一条底层读取命令。 */
        while (readable < count_left && last_fragment + 1 < fragcount) {
            struct bd_fragment *next = &fraglist[last_fragment + 1];

            if (next->count == 0 || next->sector != physical_end)
                break;

            last_offset += fraglist[last_fragment].count;
            readable += next->count;
            physical_end += next->count;
            last_fragment++;
        }

        count_read = count_left;
        if (count_read > readable) {
            count_read = readable;
            M_DEBUG("%s: clipping sectors %d -> %d\n", __FUNCTION__, count_left, count_read);
        }

        if (bd->read(bd, f->sector + (sector_start - offset), buffer, count_read) != count_read) {
            M_PRINTF("%s: ERROR: read failed!\n", __FUNCTION__);
            return -1;
        }

        bd_defrag_cursor_set(cursor, bd, fragcount, fraglist, last_fragment, last_offset);
        sector_start += count_read;
        count_left -= count_read;
        buffer = (u8 *)buffer + (count_read * bd->sectorSize);
    }

    return count;
}

int bd_defrag_read(struct block_device* bd, u32 fragcount, struct bd_fragment* fraglist, u64 sector, void* buffer, u16 count)
{
    return bd_defrag_read_cached(bd, fragcount, fraglist, sector, buffer, count, NULL);
}

int bd_defrag_write(struct block_device* bd, u32 fragcount, struct bd_fragment* fraglist, u64 sector, const void* buffer, u16 count)
{
    u64 sector_start = sector;
    u16 count_left = count;

    while (count_left > 0) {
        u16 count_write;
        u64 offset = 0; // offset of fragment in bd/file
        struct bd_fragment *f = NULL;
        int i;

        // Locate fragment containing start sector
        for (i=0; (u32)i<fragcount; i++) {
            f = &fraglist[i];
            if (offset <= sector_start && (offset + f->count) > sector_start) {
                // Fragment found
                break;
            }
            offset += f->count;
        }

        if ((u32)i == fragcount) {
            M_PRINTF("%s: ERROR: fragment not found!\n", __FUNCTION__);
            return -1;
        }

        // Clip to fragment size
        count_write = count_left;
        if ((sector_start + count_write) > (offset + f->count)) {
            count_write = (offset + f->count) - sector_start;
            M_DEBUG("%s: clipping sectors %d -> %d\n", __FUNCTION__, count_left, count_write);
        }

        // Do the write
        if (bd->write(bd, f->sector + (sector_start - offset), buffer, count_write) != count_write) {
            M_PRINTF("%s: ERROR: write failed!\n", __FUNCTION__);
            return -1;
        }

        // Advance to next fragment
        sector_start += count_write;
        count_left -= count_write;
        buffer = (u8*)buffer + (count_write * bd->sectorSize);
    }

    return count;
}
