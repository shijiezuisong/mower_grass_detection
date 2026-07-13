#include "grass_density_app.h"

#include <string.h>

#include "app_threadx.h"
#include "main.h"
#include "vl53l8cx_app.h"

#include "../libraries/easylogger/elog.h"

#define GRASS_DENSITY_THREAD_STACK_SIZE   (2048U)
#define GRASS_DENSITY_THREAD_PRIORITY     TX_THREAD_PRIORITY_UART_RX_ALGO
#define GRASS_DENSITY_LOOP_DELAY_MS       (33U)
#define GRASS_DENSITY_LOG_PERIOD          (15U)

#define GRASS_DENSITY_MIN_MM              (30U)
#define GRASS_DENSITY_MAX_MM              (2000U)
#define GRASS_DENSITY_NEAR_200_MM         (200U)
#define GRASS_DENSITY_NEAR_300_MM         (300U)
#define GRASS_DENSITY_NEAR_500_MM         (500U)
#define GRASS_DENSITY_SMOOTH_WINDOW       (5U)

static TX_THREAD s_grass_density_thread;
static ULONG s_grass_density_stack[GRASS_DENSITY_THREAD_STACK_SIZE / sizeof(ULONG)];

static grass_density_result_t s_latest;
static uint8_t s_has_latest;
static uint32_t s_eval_count;

/* Debug counters for live watch in debugger. */
static volatile uint32_t s_grass_debug_count;
static volatile uint32_t s_grass_debug_loop_count;
static volatile uint32_t s_grass_debug_no_frame_count;
static volatile uint32_t s_grass_debug_eval_fail_count;
static volatile uint32_t s_grass_debug_last_score;

static uint16_t s_score_window[GRASS_DENSITY_SMOOTH_WINDOW];
static uint8_t  s_score_window_idx;
static uint8_t  s_score_window_full;

static uint8_t grass_density_is_valid_status(uint8_t status)
{
    return (status == 5U) || (status == 9U);
}

static const char *grass_density_level_text(grass_density_level_t level)
{
    switch (level)
    {
    case GRASS_DENSITY_SPARSE:
        return "sparse";
    case GRASS_DENSITY_MEDIUM:
        return "medium";
    case GRASS_DENSITY_DENSE:
        return "dense";
    default:
        return "unknown";
    }
}

static grass_density_level_t grass_density_pick_level(uint16_t score_q100)
{
    if (score_q100 >= 70U)
    {
        return GRASS_DENSITY_DENSE;
    }
    if (score_q100 >= 40U)
    {
        return GRASS_DENSITY_MEDIUM;
    }
    return GRASS_DENSITY_SPARSE;
}

static uint16_t grass_density_smooth_score(uint16_t raw_score)
{
    uint32_t sum;
    uint8_t  count;
    uint8_t  i;

    s_score_window[s_score_window_idx] = raw_score;
    s_score_window_idx = (uint8_t)((s_score_window_idx + 1U) % GRASS_DENSITY_SMOOTH_WINDOW);

    if (s_score_window_idx == 0U)
    {
        s_score_window_full = 1U;
    }

    count = s_score_window_full ? (uint8_t)GRASS_DENSITY_SMOOTH_WINDOW : s_score_window_idx;
    if (count == 0U)
    {
        return raw_score;
    }

    sum = 0U;
    for (i = 0U; i < count; i++)
    {
        sum += s_score_window[i];
    }

    return (uint16_t)(sum / count);
}

static uint8_t grass_density_eval_from_tof(const vl53l8cx_app_frame_t *frame, grass_density_result_t *out)
{
    uint16_t i;
    uint16_t zone_count;
    uint32_t dist_sum;
    uint16_t valid_count;
    uint16_t near_200;
    uint16_t near_300;
    uint16_t near_500;
    uint16_t min_dist;
    uint16_t mean_dist;
    uint16_t valid_ratio_q100;
    uint16_t near_ratio_300_q100;
    uint16_t near_ratio_500_q100;
    uint32_t score;

    if ((frame == 0) || (out == 0))
    {
        return 0U;
    }

    zone_count = (uint16_t)frame->resolution;
    if ((zone_count == 0U) || (zone_count > VL53L8CX_APP_ZONE_COUNT))
    {
        return 0U;
    }

    dist_sum = 0U;
    valid_count = 0U;
    near_200 = 0U;
    near_300 = 0U;
    near_500 = 0U;
    min_dist = 0xFFFFU;

    for (i = 0U; i < zone_count; i++)
    {
        const uint8_t status = frame->target_status[i];
        const uint16_t dist = frame->distance_mm[i];

        if (grass_density_is_valid_status(status) == 0U)
        {
            continue;
        }

        if ((dist < GRASS_DENSITY_MIN_MM) || (dist > GRASS_DENSITY_MAX_MM))
        {
            continue;
        }

        valid_count++;
        dist_sum += dist;

        if (dist < min_dist)
        {
            min_dist = dist;
        }

        if (dist <= GRASS_DENSITY_NEAR_200_MM)
        {
            near_200++;
        }
        if (dist <= GRASS_DENSITY_NEAR_300_MM)
        {
            near_300++;
        }
        if (dist <= GRASS_DENSITY_NEAR_500_MM)
        {
            near_500++;
        }
    }

    if (valid_count == 0U)
    {
        memset(out, 0, sizeof(*out));
        out->valid = 0U;
        out->level = GRASS_DENSITY_UNKNOWN;
        out->zone_count = zone_count;
        return 0U;
    }

    mean_dist = (uint16_t)(dist_sum / valid_count);
    valid_ratio_q100 = (uint16_t)((valid_count * 100U) / zone_count);
    near_ratio_300_q100 = (uint16_t)((near_300 * 100U) / valid_count);
    near_ratio_500_q100 = (uint16_t)((near_500 * 100U) / valid_count);

    score = (uint32_t)near_ratio_300_q100 * 50U;
    score += (uint32_t)near_ratio_500_q100 * 20U;
    score += (uint32_t)valid_ratio_q100 * 30U;
    score /= 100U;

    memset(out, 0, sizeof(*out));
    out->valid = 1U;
    out->zone_count = zone_count;
    out->valid_count = valid_count;
    out->mean_distance_mm = mean_dist;
    out->min_distance_mm = min_dist;
    out->near_count_200mm = near_200;
    out->near_count_300mm = near_300;
    out->near_count_500mm = near_500;
    out->score_q100 = (uint16_t)score;
    out->score_smooth_q100 = grass_density_smooth_score((uint16_t)score);
    out->level = grass_density_pick_level(out->score_smooth_q100);

    return 1U;
}

static void grass_density_thread_entry(ULONG thread_input)
{
    static vl53l8cx_app_frame_t frame;
    static grass_density_result_t result;

    (void)thread_input;

    log_i("grass density thread started");
    s_grass_debug_count++;

    while (1)
    {
        s_grass_debug_loop_count++;

        if (vl53l8cx_app_get_frame(&frame) != 0U)
        {
            if (grass_density_eval_from_tof(&frame, &result) != 0U)
            {
                s_latest = result;
                s_has_latest = 1U;
                s_eval_count++;
                s_grass_debug_count++;
                s_grass_debug_last_score = result.score_smooth_q100;

                if ((s_eval_count % GRASS_DENSITY_LOG_PERIOD) == 0U)
                {
                    log_i("grass density level=%s score=%u smooth=%u valid=%u/%u mean=%umm min=%umm near300=%u",
                          grass_density_level_text(result.level),
                          (unsigned int)result.score_q100,
                          (unsigned int)result.score_smooth_q100,
                          (unsigned int)result.valid_count,
                          (unsigned int)result.zone_count,
                          (unsigned int)result.mean_distance_mm,
                          (unsigned int)result.min_distance_mm,
                          (unsigned int)result.near_count_300mm);
                }
            }
            else
            {
                s_grass_debug_eval_fail_count++;
            }
        }
        else
        {
            s_grass_debug_no_frame_count++;
        }

        if ((s_grass_debug_loop_count % 30U) == 0U)
        {
            log_i("grass density loop=%lu eval=%lu no_frame=%lu eval_fail=%lu",
                  (unsigned long)s_grass_debug_loop_count,
                  (unsigned long)s_eval_count,
                  (unsigned long)s_grass_debug_no_frame_count,
                  (unsigned long)s_grass_debug_eval_fail_count);
        }

        sys_delay_ms(GRASS_DENSITY_LOOP_DELAY_MS);
    }
}

void grass_density_app_thread_init(void)
{
    UINT tx_status;

    tx_status = tx_thread_create(&s_grass_density_thread,
                                 "grass_density",
                                 grass_density_thread_entry,
                                 0,
                                 s_grass_density_stack,
                                 sizeof(s_grass_density_stack),
                                 GRASS_DENSITY_THREAD_PRIORITY,
                                 GRASS_DENSITY_THREAD_PRIORITY,
                                 TX_NO_TIME_SLICE,
                                 TX_AUTO_START);
    if (tx_status != TX_SUCCESS)
    {
        log_w("grass density thread create failed, st=%u", (unsigned int)tx_status);
    }
}

uint8_t grass_density_app_get_latest(grass_density_result_t *result)
{
    if ((result == 0) || (s_has_latest == 0U))
    {
        return 0U;
    }

    *result = s_latest;
    return 1U;
}
