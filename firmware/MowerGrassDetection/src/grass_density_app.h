#ifndef GRASS_DENSITY_APP_H
#define GRASS_DENSITY_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    GRASS_DENSITY_UNKNOWN = 0,
    GRASS_DENSITY_SPARSE,
    GRASS_DENSITY_MEDIUM,
    GRASS_DENSITY_DENSE
} grass_density_level_t;

typedef struct
{
    uint8_t valid;
    grass_density_level_t level;
    uint16_t zone_count;
    uint16_t valid_count;
    uint16_t mean_distance_mm;
    uint16_t min_distance_mm;
    uint16_t near_count_200mm;
    uint16_t near_count_300mm;
    uint16_t near_count_500mm;
    uint16_t score_q100;
    uint16_t score_smooth_q100;
} grass_density_result_t;

void grass_density_app_thread_init(void);
uint8_t grass_density_app_get_latest(grass_density_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* GRASS_DENSITY_APP_H */
