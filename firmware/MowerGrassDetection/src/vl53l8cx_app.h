#ifndef VL53L8CX_APP_H
#define VL53L8CX_APP_H

#include <stdint.h>

#define VL53L8CX_APP_ZONE_COUNT    (64U)

typedef struct
{
	uint8_t  valid;
	uint8_t  resolution;
	int8_t   silicon_temp_degc;
	uint16_t distance_mm[VL53L8CX_APP_ZONE_COUNT];
	uint8_t  target_status[VL53L8CX_APP_ZONE_COUNT];
	uint32_t signal_per_spad[VL53L8CX_APP_ZONE_COUNT];
	uint32_t ambient_per_spad[VL53L8CX_APP_ZONE_COUNT];
} vl53l8cx_app_frame_t;

#ifdef __cplusplus
extern "C"
{
#endif

void vl53l8cx_app_thread_init(void);
uint8_t vl53l8cx_app_is_ready(void);
uint8_t vl53l8cx_app_get_latest(uint16_t *distance_mm, uint8_t max_count, uint8_t *valid_count);
uint8_t vl53l8cx_app_get_frame(vl53l8cx_app_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif
