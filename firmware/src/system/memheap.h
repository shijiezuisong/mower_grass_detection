#pragma once

#include <stdint.h>
#include <stdlib.h>

void memheap_init(void);

void *tx_byte_malloc(size_t size);
void  tx_byte_free(void *ptr);