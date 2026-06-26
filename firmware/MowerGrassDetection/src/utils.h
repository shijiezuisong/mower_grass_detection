#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ARRAYLEN(x) (sizeof(x) / sizeof((x)[0]))
#define ARRAYEND(x) (&(x)[ARRAYLEN(x)])

#define BYTE0(dwTemp) (*((char *)(&dwTemp)))
#define BYTE1(dwTemp) (*((char *)(&dwTemp) + 1))
#define BYTE2(dwTemp) (*((char *)(&dwTemp) + 2))
#define BYTE3(dwTemp) (*((char *)(&dwTemp) + 3))

#define LW_MAX(x, y) ((x) > (y) ? (x) : (y))
#define LW_MIN(x, y) ((x) < (y) ? (x) : (y))

int8_t utils_hex2char(uint8_t hex);
void   utils_hex2str(uint8_t *input, uint16_t input_len, char *output);

static inline uint16_t htobe16(uint16_t val)
{
    return __builtin_bswap16(val);
}

static inline uint16_t be16toh(uint16_t val)
{
    return __builtin_bswap16(val);
}

static inline uint32_t htobe32(uint32_t val)
{
    return __builtin_bswap32(val);
}

static inline uint32_t be32toh(uint32_t val)
{
    return __builtin_bswap32(val);
}
