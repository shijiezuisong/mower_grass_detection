#include "tx_api.h"
#include "memheap.h"

#define MEM_HEAP_SIZE_KB 20

static TX_BYTE_POOL mem_byte_pool;

/** 创建动态内存分配池 */
void memheap_init(void)
{
    static uint8_t _heap[1024 * MEM_HEAP_SIZE_KB];
    tx_byte_pool_create(&mem_byte_pool, "mem_byte_pool", &_heap, sizeof(_heap));
}

void *tx_byte_malloc(size_t size)
{
    void *p = NULL;
    if (TX_SUCCESS == tx_byte_allocate(&mem_byte_pool, &p, size, TX_WAIT_FOREVER))
    {
        return p;
    }

    return NULL;
}

void tx_byte_free(void *ptr)
{
    tx_byte_release(ptr);
}

void *realloc(void *ptr, size_t size)
{
    tx_byte_free(ptr);
    return tx_byte_malloc(size);
}