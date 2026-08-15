#include <bd_cache.h>
#include <string.h>
#include <sysmem.h>

//#define DEBUG  //comment out this line when not debugging
#include "module_debug.h"

#define CACHE_TARGET_BLOCK_SIZE 4096
#define CACHE_TOTAL_SIZE        (128 * 1024)
#define BLOCK_COUNT_MAX         32
#define BLOCK_WEIGHT_FACTOR     256 // Fixed point math (24.8)
#define INVALID_SECTOR          0xffffffffffffffff

struct bd_cache
{
    struct block_device *bd;
    u32 sector_size;
    u32 block_size;
    u16 sectors_per_block;
    u16 block_count;
    int weight[BLOCK_COUNT_MAX];
    u64 sector[BLOCK_COUNT_MAX];
    u8 *cache;
#ifdef DEBUG
    u32 sectors_read;
    u32 sectors_cache;
    u32 sectors_dev;
#endif
};

static u8 *_get_block(struct bd_cache *c, int blkidx)
{
    return c->cache + blkidx * c->block_size;
}

/* cache overlaps with requested area ? */
static int _overlaps(struct bd_cache *c, u64 csector, u64 sector, u16 count)
{
    if (csector == INVALID_SECTOR || count == 0)
        return 0;

    if (sector >= csector)
        return sector - csector < c->sectors_per_block;

    return csector - sector < count;
}

/* cache contains requested area ? */
static int _contains(struct bd_cache *c, u64 csector, u64 sector, u16 count)
{
    u64 offset;

    if (csector == INVALID_SECTOR || sector < csector)
        return 0;

    offset = sector - csector;
    return offset < c->sectors_per_block && count <= c->sectors_per_block - offset;
}

static void _invalidate(struct bd_cache *c, u64 sector, u16 count)
{
    int blkidx;

    for (blkidx = 0; blkidx < c->block_count; blkidx++) {
        if (_overlaps(c, c->sector[blkidx], sector, count)) {
            // Invalidate cache entry
            c->sector[blkidx] = INVALID_SECTOR;
        }
    }
}

static int _read(struct block_device *bd, u64 sector, void *buffer, u16 count)
{
    struct bd_cache *c = bd->priv;
    int result;

    DEBUG_U64_2XU32(sector);
    M_DEBUG("%s(0x%08x%08x, %d)\n", __FUNCTION__, sector_u32[1], sector_u32[0], count);

    if (count == 0)
        return 0;

    if (count > c->sectors_per_block ||
        (count == c->sectors_per_block && c->sectors_per_block > 1) ||
        sector >= c->bd->sectorCount ||
        c->sectors_per_block > c->bd->sectorCount - sector) {
        // Do a direct read
        return c->bd->read(c->bd, sector, buffer, count);
    }

#ifdef DEBUG
    c->sectors_read += count;
#endif

    // Do a cached read
    int blkidx;
    for (blkidx = 0; blkidx < c->block_count; blkidx++) {
        if (_contains(c, c->sector[blkidx], sector, count)) {
#ifdef DEBUG
            c->sectors_cache += count;
            //M_DEBUG("- CACHE HIT[%d] [block %d] [devread %ds, hit-ratio %d%%]\n", sector, blkidx, c->sectors_dev, (c->sectors_cache * 100) / c->sectors_read);
#endif
            // Minimum weight
            if (c->weight[blkidx] < 0)
                c->weight[blkidx] = 0;

            c->weight[blkidx] += count * BLOCK_WEIGHT_FACTOR;

            // Read from cache
            u32 offset = (sector - c->sector[blkidx]) * c->sector_size;
            memcpy(buffer, _get_block(c, blkidx) + offset, count * c->sector_size);
            return count;
        }
    }

    // Find block with the lowest weight
    int blkidx_best_weight = 0x7fffffff;
    int blkidx_best = 0;
    M_DEBUG("- list: ");
    for (blkidx = 0; blkidx < c->block_count; blkidx++) {
#ifdef DEBUG
        printf("%*d ", 3, c->weight[blkidx] / BLOCK_WEIGHT_FACTOR);
#endif

        // Dynamic aging
        c->weight[blkidx] -= (c->sectors_per_block * BLOCK_WEIGHT_FACTOR / c->block_count) + (c->weight[blkidx] / 32);

        if (c->weight[blkidx] < blkidx_best_weight) {
            // Better block found
            blkidx_best_weight = c->weight[blkidx];
            blkidx_best = blkidx;
        }
    }
#ifdef DEBUG
    printf(" devread: %*d, evict %*d [%*d], add [%*d]\n", 4, c->sectors_dev, 2, blkidx_best, 8, c->sector[blkidx_best], 8, sector);
    c->sectors_dev += c->sectors_per_block;
    //M_DEBUG("- CACHE READ[%d] -> [block %d] [devread %ds, hit-ratio %d%%]\n", sector, blkidx_best, c->sectors_dev, (c->sectors_cache * 100) / c->sectors_read);
#endif

    // 读取完整数据前保持缓存项无效，防止返回陈旧或不完整的数据。
    c->sector[blkidx_best] = INVALID_SECTOR;
    result = c->bd->read(c->bd, sector, _get_block(c, blkidx_best), c->sectors_per_block);
    if (result != c->sectors_per_block) {
        // 预读范围内、调用方实际未请求的扇区也可能导致读取失败。
        // 此时仅重读实际请求的扇区，并保持缓存项无效。
        return c->bd->read(c->bd, sector, buffer, count);
    }

    // 只有完整读取缓存块后，才发布这个缓存项。
    c->sector[blkidx_best] = sector;

    // Read from cache
    u32 offset = (sector - c->sector[blkidx_best]) * c->sector_size;
    c->weight[blkidx_best] = count * BLOCK_WEIGHT_FACTOR;
    memcpy(buffer, _get_block(c, blkidx_best) + offset, count * c->sector_size);
    return count;
}

static int _write(struct block_device *bd, u64 sector, const void *buffer, u16 count)
{
    struct bd_cache *c = bd->priv;

    DEBUG_U64_2XU32(sector);
    M_DEBUG("%s(0x%08x%08x, %d)\n", __FUNCTION__, sector_u32[1], sector_u32[0], count);

    _invalidate(c, sector, count);

    return c->bd->write(c->bd, sector, buffer, count);
}

static void _flush(struct block_device *bd)
{
    struct bd_cache *c = bd->priv;

    M_DEBUG("%s\n", __FUNCTION__);

    return c->bd->flush(c->bd);
}

static int _stop(struct block_device *bd)
{
    struct bd_cache *c = bd->priv;

    M_DEBUG("%s\n", __FUNCTION__);

    return c->bd->stop(c->bd);
}

struct block_device *bd_cache_create(struct block_device *bd)
{
    int blkidx;
    u32 sectors_per_block;
    u32 block_size;
    u32 block_count;

    // 缓存支持 512～8192 字节的二次幂逻辑扇区；FatFs 当前只挂载到 4096 字节。
    if (bd == NULL || bd->sectorSize < 512 || bd->sectorSize > 8192 || (bd->sectorSize & (bd->sectorSize - 1)) != 0)
        return NULL;

    if (bd->sectorSize <= CACHE_TARGET_BLOCK_SIZE)
        sectors_per_block = CACHE_TARGET_BLOCK_SIZE / bd->sectorSize;
    else
        sectors_per_block = 1;

    block_size = sectors_per_block * bd->sectorSize;
    block_count = CACHE_TOTAL_SIZE / block_size;
    if (block_count > BLOCK_COUNT_MAX)
        block_count = BLOCK_COUNT_MAX;
    if (block_count == 0)
        return NULL;

    // Create new block device
    struct block_device *cbd = AllocSysMemory(ALLOC_FIRST, sizeof(struct block_device), NULL);
    // Create new private data
    struct bd_cache *c = AllocSysMemory(ALLOC_FIRST, sizeof(struct bd_cache), NULL);

    M_DEBUG("%s\n", __FUNCTION__);

    if (cbd == NULL || c == NULL) {
        if (c != NULL)
            FreeSysMemory(c);
        if (cbd != NULL)
            FreeSysMemory(cbd);
        return NULL;
    }

    c->cache = AllocSysMemory(ALLOC_FIRST, block_size * block_count, NULL);
    if (c->cache == NULL) {
        FreeSysMemory(c);
        FreeSysMemory(cbd);
        return NULL;
    }

    c->bd = bd;
    c->sector_size = bd->sectorSize;
    c->block_size = block_size;
    c->sectors_per_block = sectors_per_block;
    c->block_count = block_count;
    for (blkidx = 0; blkidx < c->block_count; blkidx++) {
        c->weight[blkidx] = 0;
        c->sector[blkidx] = INVALID_SECTOR;
    }
#ifdef DEBUG
    c->sectors_read = 0;
    c->sectors_cache = 0;
    c->sectors_dev = 0;
#endif

    // copy all parameters becouse we are the same blocks device
    // only difference is we are cached.
    cbd->priv         = c;
    cbd->name         = bd->name;
    cbd->devNr        = bd->devNr;
    cbd->parNr        = bd->parNr;
    cbd->parId        = bd->parId;
    cbd->sectorSize   = bd->sectorSize;
    cbd->sectorOffset = bd->sectorOffset;
    cbd->sectorCount  = bd->sectorCount;

    cbd->read = _read;
    cbd->write = _write;
    cbd->flush = _flush;
    cbd->stop = _stop;

    return cbd;
}

void bd_cache_destroy(struct block_device *cbd)
{
    struct bd_cache *c;

    M_DEBUG("%s\n", __FUNCTION__);

    if (cbd == NULL)
        return;

    c = cbd->priv;
    if (c != NULL) {
        if (c->cache != NULL)
            FreeSysMemory(c->cache);
        FreeSysMemory(c);
    }
    FreeSysMemory(cbd);
}
