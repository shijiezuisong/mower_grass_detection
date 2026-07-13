#include "vl53l8cx_app.h"

#include <stdio.h>
#include <string.h>

#include "app_threadx.h"
#include "main.h"
#include "usart.h"
#include "stm32f4xx_nucleo_bus.h"

#include "../driver/drv_uart.h"

#include "../libraries/easylogger/elog.h"
#include "../libraries/vl53l8cx/modules/vl53l8cx_api.h"

#define VL53L8CX_APP_THREAD_STACK_SIZE   (4096U)
#define VL53L8CX_APP_THREAD_PRIORITY     TX_THREAD_PRIORITY_UART_RX_LOG
#define VL53L8CX_APP_RETRY_DELAY_MS      (2000U)
#define VL53L8CX_APP_POLL_DELAY_MS       (5U)
#define VL53L8CX_APP_RESOLUTION          VL53L8CX_RESOLUTION_8X8
#define VL53L8CX_APP_FREQ_HZ             (30U)
#define VL53L8CX_APP_BOOT_DELAY_MS       (10U)
#define VL53L8CX_APP_RESET_PULSE_MS      (2U)
#define VL53L8CX_APP_I2C_ADDRESS         VL53L8CX_DEFAULT_I2C_ADDRESS
#define VL53L8CX_APP_I2C_READY_TRIALS    (3U)
#define VL53L8CX_APP_I2C_READY_RETRY     (5U)
#define VL53L8CX_APP_I2C_MAX_RETRY       (3U)
#define VL53L8CX_APP_I2C_RETRY_DELAY_MS  (2U)
#define VL53L8CX_APP_I2C_WRITE_CHUNK      (256U)
#define VL53L8CX_APP_LOG_BUF_SIZE        (4096U)
#define VL53L8CX_APP_FRAME_LOG_PERIOD    (0U)
#define VL53L8CX_APP_LINUX_UART_BAUD     (921600U)
#define VL53L8CX_APP_LINUX_TX_BUF_SIZE   (3072U)

static TX_THREAD s_vl53l8cx_thread;
static ULONG     s_vl53l8cx_stack[VL53L8CX_APP_THREAD_STACK_SIZE / sizeof(ULONG)];

static VL53L8CX_Configuration s_dev;
static VL53L8CX_ResultsData   s_results;
static uint8_t                s_ready;
static uint8_t                s_has_frame;
static uint8_t                s_header_logged;
static uint32_t               s_frame_id;
static uint32_t               s_linux_tx_count;
static uint32_t               s_tof_new_frame_count;

/* Debug counters for live watch in debugger. */
static volatile uint32_t      s_vl53l8cx_debug_count;
static volatile uint32_t      s_vl53l8cx_debug_stage;
static volatile uint32_t      s_vl53l8cx_debug_loop_count;
static volatile uint32_t      s_vl53l8cx_debug_init_retry_count;
static volatile uint32_t      s_vl53l8cx_debug_data_ready_count;
static volatile uint32_t      s_vl53l8cx_debug_get_data_count;
static volatile uint32_t      s_vl53l8cx_debug_error_count;
static volatile uint32_t      s_vl53l8cx_debug_last_status;
static volatile uint32_t      s_vl53l8cx_debug_i2c_write_enter_count;
static volatile uint32_t      s_vl53l8cx_debug_i2c_write_exit_count;
static volatile uint32_t      s_vl53l8cx_debug_i2c_read_enter_count;
static volatile uint32_t      s_vl53l8cx_debug_i2c_read_exit_count;
static volatile uint32_t      s_vl53l8cx_debug_i2c_last_reg;
static volatile uint32_t      s_vl53l8cx_debug_i2c_last_len;
static volatile uint32_t      s_vl53l8cx_debug_i2c_last_op;
static volatile uint32_t      s_vl53l8cx_debug_i2c_last_bus_status;
static volatile uint32_t      s_vl53l8cx_debug_i2c_retry_count;
static volatile uint32_t      s_vl53l8cx_debug_i2c_recover_count;
static volatile uint32_t      s_vl53l8cx_debug_i2c_fail_count;
static volatile uint32_t      s_vl53l8cx_debug_i2c_ready_retry_count;
static volatile uint32_t      s_vl53l8cx_debug_i2c_chunk_write_count;

static void vl53l8cx_app_log_frame(void);
static void vl53l8cx_app_send_linux_frame(void);

static void vl53l8cx_app_raw_trace(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    (void)HAL_UART_Transmit(&huart2, (uint8_t *)text, (uint16_t)strlen(text), 20U);
}

static int32_t vl53l8cx_bus_init(void)
{
    HAL_GPIO_WritePin(TOFEN_GPIO_Port, TOFEN_Pin, GPIO_PIN_RESET);
    sys_delay_ms(VL53L8CX_APP_RESET_PULSE_MS);
    HAL_GPIO_WritePin(TOFEN_GPIO_Port, TOFEN_Pin, GPIO_PIN_SET);
    sys_delay_ms(VL53L8CX_APP_BOOT_DELAY_MS);
    return BSP_I2C1_Init();
}

static int32_t vl53l8cx_bus_deinit(void)
{
    return BSP_I2C1_DeInit();
}

static int32_t vl53l8cx_bus_get_tick(void)
{
    return (int32_t)sys_get_ms();
}

static int32_t vl53l8cx_wait_i2c_ready(uint16_t addr)
{
    uint8_t retry;

    for (retry = 0U; retry < (uint8_t)VL53L8CX_APP_I2C_READY_RETRY; retry++)
    {
        if (BSP_I2C1_IsReady(addr, VL53L8CX_APP_I2C_READY_TRIALS) == BSP_ERROR_NONE)
        {
            return 0;
        }

        s_vl53l8cx_debug_i2c_ready_retry_count++;
        sys_delay_ms(VL53L8CX_APP_I2C_RETRY_DELAY_MS);
    }

    return -1;
}

static int32_t vl53l8cx_i2c_recover_bus(void)
{
    int32_t deinit_status;
    int32_t init_status;

    deinit_status = BSP_I2C1_DeInit();
    sys_delay_ms(VL53L8CX_APP_I2C_RETRY_DELAY_MS);
    init_status = BSP_I2C1_Init();

    if ((deinit_status != BSP_ERROR_NONE) || (init_status != BSP_ERROR_NONE))
    {
        return -1;
    }

    s_vl53l8cx_debug_i2c_recover_count++;
    return 0;
}

static int32_t vl53l8cx_i2c_xfer_with_retry(uint8_t is_read, uint16_t addr, uint16_t reg, uint8_t *data, uint16_t len)
{
    uint8_t attempt;
    int32_t bus_status;

    for (attempt = 0U; attempt < (uint8_t)VL53L8CX_APP_I2C_MAX_RETRY; attempt++)
    {
        if (is_read != 0U)
        {
            bus_status = BSP_I2C1_ReadReg16(addr, reg, data, len);
        }
        else
        {
            bus_status = BSP_I2C1_WriteReg16(addr, reg, data, len);
        }

        s_vl53l8cx_debug_i2c_last_bus_status = (uint32_t)bus_status;
        if (bus_status == BSP_ERROR_NONE)
        {
            return 0;
        }

        s_vl53l8cx_debug_i2c_fail_count++;
        if ((attempt + 1U) < (uint8_t)VL53L8CX_APP_I2C_MAX_RETRY)
        {
            s_vl53l8cx_debug_i2c_retry_count++;
            (void)vl53l8cx_i2c_recover_bus();
            sys_delay_ms(VL53L8CX_APP_I2C_RETRY_DELAY_MS);
        }
    }

    return -1;
}

static int32_t vl53l8cx_i2c_write_chunked(uint16_t addr, uint16_t reg, uint8_t *data, uint16_t len)
{
    uint16_t offset = 0U;
    char trace_buf[96];

    if (len >= VL53L8CX_APP_I2C_WRITE_CHUNK)
    {
        (void)snprintf(trace_buf,
                       sizeof(trace_buf),
                       "[VL53] wr chunk start reg=0x%04X len=%u\r\n",
                       (unsigned int)reg,
                       (unsigned int)len);
        vl53l8cx_app_raw_trace(trace_buf);
    }

    while (offset < len)
    {
        uint16_t chunk_len = (uint16_t)(len - offset);

        if (chunk_len > VL53L8CX_APP_I2C_WRITE_CHUNK)
        {
            chunk_len = VL53L8CX_APP_I2C_WRITE_CHUNK;
        }

        if (vl53l8cx_i2c_xfer_with_retry(0U,
                                         addr,
                                         (uint16_t)(reg + offset),
                                         &data[offset],
                                         chunk_len) != 0)
        {
            (void)snprintf(trace_buf,
                           sizeof(trace_buf),
                           "[VL53] wr chunk fail reg=0x%04X off=%u len=%u\r\n",
                           (unsigned int)reg,
                           (unsigned int)offset,
                           (unsigned int)chunk_len);
            vl53l8cx_app_raw_trace(trace_buf);
            return -1;
        }

        s_vl53l8cx_debug_i2c_chunk_write_count++;
        if ((len >= VL53L8CX_APP_I2C_WRITE_CHUNK)
            && ((s_vl53l8cx_debug_i2c_chunk_write_count <= 4U)
                || ((s_vl53l8cx_debug_i2c_chunk_write_count % 64U) == 0U)))
        {
            (void)snprintf(trace_buf,
                           sizeof(trace_buf),
                           "[VL53] wr chunk ok cnt=%lu off=%u/%u\r\n",
                           (unsigned long)s_vl53l8cx_debug_i2c_chunk_write_count,
                           (unsigned int)(offset + chunk_len),
                           (unsigned int)len);
            vl53l8cx_app_raw_trace(trace_buf);
        }

        offset = (uint16_t)(offset + chunk_len);
    }

    if (len >= VL53L8CX_APP_I2C_WRITE_CHUNK)
    {
        (void)snprintf(trace_buf,
                       sizeof(trace_buf),
                       "[VL53] wr chunk done reg=0x%04X len=%u\r\n",
                       (unsigned int)reg,
                       (unsigned int)len);
        vl53l8cx_app_raw_trace(trace_buf);
    }

    return 0;
}

static int32_t vl53l8cx_i2c_write_reg(uint16_t addr, uint16_t reg, uint8_t *data, uint16_t len)
{
    s_vl53l8cx_debug_i2c_last_op = 1U;
    s_vl53l8cx_debug_i2c_last_reg = reg;
    s_vl53l8cx_debug_i2c_last_len = len;
    s_vl53l8cx_debug_i2c_write_enter_count++;

    if ((data == 0) && (len > 0U))
    {
        s_vl53l8cx_debug_i2c_last_bus_status = 0xFFFFFFFFU;
        return -1;
    }

    s_vl53l8cx_debug_i2c_write_exit_count++;

    if (vl53l8cx_i2c_write_chunked(addr, reg, data, len) != 0)
    {
        if (s_vl53l8cx_debug_i2c_fail_count <= 5U)
        {
            log_w("i2c wr fail reg=0x%04lX len=%lu st=%ld retry=%lu rec=%lu",
                  (unsigned long)reg,
                  (unsigned long)len,
                  (long)s_vl53l8cx_debug_i2c_last_bus_status,
                  (unsigned long)s_vl53l8cx_debug_i2c_retry_count,
                  (unsigned long)s_vl53l8cx_debug_i2c_recover_count);
        }
        return -1;
    }

    if (s_vl53l8cx_debug_i2c_write_exit_count <= 3U)
    {
        log_i("i2c wr#%lu reg=0x%04lX len=%lu st=%ld",
              (unsigned long)s_vl53l8cx_debug_i2c_write_exit_count,
              (unsigned long)reg,
              (unsigned long)len,
              (long)s_vl53l8cx_debug_i2c_last_bus_status);
    }

    return 0;
}

static int32_t vl53l8cx_i2c_read_reg(uint16_t addr, uint16_t reg, uint8_t *data, uint16_t len)
{
    s_vl53l8cx_debug_i2c_last_op = 2U;
    s_vl53l8cx_debug_i2c_last_reg = reg;
    s_vl53l8cx_debug_i2c_last_len = len;
    s_vl53l8cx_debug_i2c_read_enter_count++;

    if ((data == 0) && (len > 0U))
    {
        s_vl53l8cx_debug_i2c_last_bus_status = 0xFFFFFFFFU;
        return -1;
    }

    s_vl53l8cx_debug_i2c_read_exit_count++;

    if (vl53l8cx_i2c_xfer_with_retry(1U, addr, reg, data, len) != 0)
    {
        if (s_vl53l8cx_debug_i2c_fail_count <= 5U)
        {
            log_w("i2c rd fail reg=0x%04lX len=%lu st=%ld retry=%lu rec=%lu",
                  (unsigned long)reg,
                  (unsigned long)len,
                  (long)s_vl53l8cx_debug_i2c_last_bus_status,
                  (unsigned long)s_vl53l8cx_debug_i2c_retry_count,
                  (unsigned long)s_vl53l8cx_debug_i2c_recover_count);
        }
        return -1;
    }

    return 0;
}

static int8_t vl53l8cx_app_sensor_start(void)
{
    uint8_t status;
    uint8_t is_alive = 0U;

    vl53l8cx_app_raw_trace("[VL53] sensor_start begin\r\n");

    s_vl53l8cx_debug_stage = 1U;
    s_vl53l8cx_debug_count++;
    log_i("vl53l8cx sensor start begin");

    if (vl53l8cx_bus_init() != 0)
    {
        vl53l8cx_app_raw_trace("[VL53] bus_init fail\r\n");
        s_vl53l8cx_debug_last_status = 1001U;
        s_vl53l8cx_debug_error_count++;
        log_w("vl53l8cx I2C1 bus init failed");
        return -1;
    }
    vl53l8cx_app_raw_trace("[VL53] bus_init ok\r\n");

    s_vl53l8cx_debug_stage = 2U;
    s_vl53l8cx_debug_count++;
    log_i("vl53l8cx I2C1 bus init ok");

    s_vl53l8cx_debug_stage = 3U;
    s_vl53l8cx_debug_count++;
    if (vl53l8cx_wait_i2c_ready(VL53L8CX_APP_I2C_ADDRESS) != 0)
    {
        vl53l8cx_app_raw_trace("[VL53] i2c not ready\r\n");
        s_vl53l8cx_debug_last_status = 1002U;
        s_vl53l8cx_debug_error_count++;
        log_w("vl53l8cx I2C1 not ready addr=0x%02X", VL53L8CX_APP_I2C_ADDRESS);
        (void)vl53l8cx_bus_deinit();
        return -1;
    }
    vl53l8cx_app_raw_trace("[VL53] i2c ready\r\n");
    log_i("vl53l8cx I2C1 ready addr=0x%02X", VL53L8CX_APP_I2C_ADDRESS);

    s_vl53l8cx_debug_stage = 4U;
    s_vl53l8cx_debug_count++;

    memset(&s_dev, 0, sizeof(s_dev));
    s_dev.platform.address = VL53L8CX_APP_I2C_ADDRESS;
    s_dev.platform.Write = vl53l8cx_i2c_write_reg;
    s_dev.platform.Read = vl53l8cx_i2c_read_reg;
    s_dev.platform.GetTick = vl53l8cx_bus_get_tick;

    s_vl53l8cx_debug_stage = 5U;
    s_vl53l8cx_debug_count++;
    log_i("vl53l8cx probe begin");
    status = vl53l8cx_is_alive(&s_dev, &is_alive);
    s_vl53l8cx_debug_last_status = status;
    if ((status != VL53L8CX_STATUS_OK) || (is_alive == 0U))
    {
        vl53l8cx_app_raw_trace("[VL53] probe fail\r\n");
        s_vl53l8cx_debug_error_count++;
        log_w("vl53l8cx probe failed on I2C1, st=%u alive=%u", status, is_alive);
        (void)vl53l8cx_bus_deinit();
        return -1;
    }
    vl53l8cx_app_raw_trace("[VL53] probe ok\r\n");

    s_vl53l8cx_debug_stage = 6U;
    s_vl53l8cx_debug_count++;
    log_i("vl53l8cx init begin");
    vl53l8cx_app_raw_trace("[VL53] call init\r\n");
    status = vl53l8cx_init(&s_dev);
    {
        char trace_buf[64];
        (void)snprintf(trace_buf, sizeof(trace_buf), "[VL53] init ret=%u\r\n", (unsigned int)status);
        vl53l8cx_app_raw_trace(trace_buf);
    }
    s_vl53l8cx_debug_last_status = status;
    if (status != VL53L8CX_STATUS_OK)
    {
        vl53l8cx_app_raw_trace("[VL53] init fail\r\n");
        s_vl53l8cx_debug_error_count++;
        log_w("vl53l8cx_init failed on I2C1, st=%u", status);
        (void)vl53l8cx_bus_deinit();
        return -1;
    }
    vl53l8cx_app_raw_trace("[VL53] init ok\r\n");

    s_vl53l8cx_debug_stage = 7U;
    s_vl53l8cx_debug_count++;
    log_i("vl53l8cx init ok");

    s_vl53l8cx_debug_stage = 8U;
    s_vl53l8cx_debug_count++;
    log_i("vl53l8cx alive on I2C1 addr=0x%02X", VL53L8CX_APP_I2C_ADDRESS);

    status = vl53l8cx_set_resolution(&s_dev, VL53L8CX_APP_RESOLUTION);
    s_vl53l8cx_debug_last_status = status;
    if (status != VL53L8CX_STATUS_OK)
    {
        s_vl53l8cx_debug_error_count++;
        log_w("vl53l8cx_set_resolution failed, st=%u", status);
        (void)vl53l8cx_bus_deinit();
        return -1;
    }

    s_vl53l8cx_debug_stage = 9U;
    s_vl53l8cx_debug_count++;
    status = vl53l8cx_set_ranging_frequency_hz(&s_dev, VL53L8CX_APP_FREQ_HZ);
    s_vl53l8cx_debug_last_status = status;
    if (status != VL53L8CX_STATUS_OK)
    {
        s_vl53l8cx_debug_error_count++;
        log_w("vl53l8cx_set_freq failed, st=%u", status);
        (void)vl53l8cx_bus_deinit();
        return -1;
    }

    s_vl53l8cx_debug_stage = 10U;
    s_vl53l8cx_debug_count++;
    status = vl53l8cx_set_ranging_mode(&s_dev, VL53L8CX_RANGING_MODE_CONTINUOUS);
    s_vl53l8cx_debug_last_status = status;
    if (status != VL53L8CX_STATUS_OK)
    {
        s_vl53l8cx_debug_error_count++;
        log_w("vl53l8cx_set_mode failed, st=%u", status);
        (void)vl53l8cx_bus_deinit();
        return -1;
    }

    s_vl53l8cx_debug_stage = 11U;
    s_vl53l8cx_debug_count++;
    status = vl53l8cx_start_ranging(&s_dev);
    s_vl53l8cx_debug_last_status = status;
    if (status != VL53L8CX_STATUS_OK)
    {
        vl53l8cx_app_raw_trace("[VL53] start fail\r\n");
        s_vl53l8cx_debug_error_count++;
        log_w("vl53l8cx_start_ranging failed, st=%u", status);
        (void)vl53l8cx_bus_deinit();
        return -1;
    }
    vl53l8cx_app_raw_trace("[VL53] start ok\r\n");

    s_ready = 1U;
    s_header_logged = 0U;
    s_vl53l8cx_debug_stage = 12U;
    s_vl53l8cx_debug_count++;
    log_i("vl53l8cx started on I2C1");
    return 0;
}

static void thread_vl53l8cx_entry(ULONG thread_input)
{
    uint8_t data_ready = 0U;

    (void)thread_input;

    vl53l8cx_app_raw_trace("[VL53] thread entry\r\n");
    log_i("vl53l8cx thread started");
    s_vl53l8cx_debug_stage = 20U;
    s_vl53l8cx_debug_count++;

    /* Initialize USART6 here, after ThreadX kernel is fully running. */
    vl53l8cx_app_raw_trace("[VL53] usart6 init begin\r\n");
    bsp_uart_init(USART6_INSTANCE,
                  VL53L8CX_APP_LINUX_UART_BAUD,
                  UART_WORDLENGTH_8B,
                  UART_STOPBITS_1,
                  UART_PARITY_NONE);
    vl53l8cx_app_raw_trace("[VL53] usart6 init done\r\n");
    log_i("vl53l8cx usart6 init done");

    while (1)
    {
        s_vl53l8cx_debug_loop_count++;

        if (s_ready == 0U)
        {
            s_vl53l8cx_debug_stage = 21U;
            if (vl53l8cx_app_sensor_start() != 0)
            {
                s_vl53l8cx_debug_init_retry_count++;
                log_w("vl53l8cx init/start failed, retrying");
                sys_delay_ms(VL53L8CX_APP_RETRY_DELAY_MS);
                continue;
            }
        }

        data_ready = 0U;
        if (vl53l8cx_check_data_ready(&s_dev, &data_ready) == VL53L8CX_STATUS_OK)
        {
            if ((data_ready > 0U) && (vl53l8cx_get_ranging_data(&s_dev, &s_results) == VL53L8CX_STATUS_OK))
            {
                s_vl53l8cx_debug_stage = 22U;
                s_vl53l8cx_debug_data_ready_count++;
                s_vl53l8cx_debug_get_data_count++;
                s_has_frame = 1U;
                s_frame_id++;
                s_tof_new_frame_count++;
                vl53l8cx_app_send_linux_frame();

                if ((VL53L8CX_APP_FRAME_LOG_PERIOD > 0U)
                    && ((s_tof_new_frame_count % VL53L8CX_APP_FRAME_LOG_PERIOD) == 0U))
                {
                    vl53l8cx_app_log_frame();
                }

                if ((s_tof_new_frame_count % 30U) == 0U)
                {
                    log_i("tof new_frame count=%lu frame_id=%lu", (unsigned long)s_tof_new_frame_count, (unsigned long)s_frame_id);
                }
            }
        }
        else
        {
            s_vl53l8cx_debug_stage = 23U;
            s_vl53l8cx_debug_error_count++;
            s_ready = 0U;
            s_has_frame = 0U;
            (void)vl53l8cx_stop_ranging(&s_dev);
            (void)vl53l8cx_bus_deinit();
            sys_delay_ms(VL53L8CX_APP_RETRY_DELAY_MS);
            continue;
        }

        sys_delay_ms(VL53L8CX_APP_POLL_DELAY_MS);
    }
}

static void vl53l8cx_app_log_frame(void)
{
    char     log_buf[VL53L8CX_APP_LOG_BUF_SIZE];
    int      written;
    uint8_t  side;
    uint8_t  row;
    uint8_t  col;
    uint8_t  zone;
    uint8_t  zone_count;
    uint32_t signal;
    uint32_t ambient;
    int16_t  distance;
    uint8_t  status;
    size_t   offset;
    size_t   remaining;

    offset = 0U;
    remaining = sizeof(log_buf);
    zone_count = (VL53L8CX_APP_RESOLUTION == VL53L8CX_RESOLUTION_4X4) ? 16U : 64U;
    side = (zone_count == 16U) ? 4U : 8U;

    if (s_header_logged == 0U)
    {
        written = snprintf(log_buf,
                           remaining,
                           "Cell Format:\n"
                           "Distance [mm] : Status\n"
                           "Signal [kcps/spad] : Ambient [kcps/spad]\n"
                           "------------------------------------------------------------\n");
        if ((written < 0) || ((size_t)written >= remaining))
        {
            return;
        }

        offset += (size_t)written;
        remaining -= (size_t)written;
        s_header_logged = 1U;
    }

    for (row = 0U; row < side; row++)
    {
        for (col = 0U; col < side; col++)
        {
            zone = (uint8_t)((row * side) + col);
            distance = s_results.distance_mm[zone];
            status = s_results.target_status[zone];
            signal = s_results.signal_per_spad[zone];
            ambient = s_results.ambient_per_spad[zone];

            written = snprintf(log_buf + offset,
                               remaining,
                               "%4d : %2u\n%4lu : %2lu%s",
                               (int)distance,
                               (unsigned int)status,
                               (unsigned long)signal,
                               (unsigned long)ambient,
                               (col == (uint8_t)(side - 1U)) ? "\n" : " | ");
            if ((written < 0) || ((size_t)written >= remaining))
            {
                return;
            }

            offset += (size_t)written;
            remaining -= (size_t)written;
        }

        written = snprintf(log_buf + offset,
                           remaining,
                           "------------------------------------------------------------\n");
        if ((written < 0) || ((size_t)written >= remaining))
        {
            return;
        }

        offset += (size_t)written;
        remaining -= (size_t)written;
    }

    log_i("\n%s", log_buf);
}

static void vl53l8cx_app_send_linux_frame(void)
{
    char     tx_buf[VL53L8CX_APP_LINUX_TX_BUF_SIZE];
    int      written;
    uint8_t  i;
    uint8_t  zone_count;
    uint32_t ts_ms;
    size_t   offset;
    size_t   remain;

    zone_count = (VL53L8CX_APP_RESOLUTION == VL53L8CX_RESOLUTION_4X4) ? 16U : 64U;
    ts_ms = sys_get_ms();

    offset = 0U;
    remain = sizeof(tx_buf);

    written = snprintf(tx_buf + offset,
                       remain,
                       "TOF,%lu,%lu,%u,%u,D,",
                       (unsigned long)s_frame_id,
                       (unsigned long)ts_ms,
                       (unsigned int)VL53L8CX_APP_RESOLUTION,
                       (unsigned int)VL53L8CX_APP_FREQ_HZ);
    if ((written < 0) || ((size_t)written >= remain))
    {
        return;
    }
    offset += (size_t)written;
    remain -= (size_t)written;

    for (i = 0U; i < zone_count; i++)
    {
        written = snprintf(tx_buf + offset,
                           remain,
                           "%s%d",
                           (i == 0U) ? "" : ",",
                           (int)s_results.distance_mm[i]);
        if ((written < 0) || ((size_t)written >= remain))
        {
            return;
        }
        offset += (size_t)written;
        remain -= (size_t)written;
    }

    written = snprintf(tx_buf + offset, remain, ",S,");
    if ((written < 0) || ((size_t)written >= remain))
    {
        return;
    }
    offset += (size_t)written;
    remain -= (size_t)written;

    for (i = 0U; i < zone_count; i++)
    {
        written = snprintf(tx_buf + offset,
                           remain,
                           "%s%u",
                           (i == 0U) ? "" : ",",
                           (unsigned int)s_results.target_status[i]);
        if ((written < 0) || ((size_t)written >= remain))
        {
            return;
        }
        offset += (size_t)written;
        remain -= (size_t)written;
    }

    written = snprintf(tx_buf + offset, remain, ",G,");
    if ((written < 0) || ((size_t)written >= remain))
    {
        return;
    }
    offset += (size_t)written;
    remain -= (size_t)written;

    for (i = 0U; i < zone_count; i++)
    {
        written = snprintf(tx_buf + offset,
                           remain,
                           "%s%lu",
                           (i == 0U) ? "" : ",",
                           (unsigned long)s_results.signal_per_spad[i]);
        if ((written < 0) || ((size_t)written >= remain))
        {
            return;
        }
        offset += (size_t)written;
        remain -= (size_t)written;
    }

    written = snprintf(tx_buf + offset, remain, ",A,");
    if ((written < 0) || ((size_t)written >= remain))
    {
        return;
    }
    offset += (size_t)written;
    remain -= (size_t)written;

    for (i = 0U; i < zone_count; i++)
    {
        written = snprintf(tx_buf + offset,
                           remain,
                           "%s%lu",
                           (i == 0U) ? "" : ",",
                           (unsigned long)s_results.ambient_per_spad[i]);
        if ((written < 0) || ((size_t)written >= remain))
        {
            return;
        }
        offset += (size_t)written;
        remain -= (size_t)written;
    }

    written = snprintf(tx_buf + offset, remain, ",V,");
    if ((written < 0) || ((size_t)written >= remain))
    {
        return;
    }
    offset += (size_t)written;
    remain -= (size_t)written;

    for (i = 0U; i < zone_count; i++)
    {
        const uint8_t valid = (s_results.target_status[i] == 5U) || (s_results.target_status[i] == 9U);

        written = snprintf(tx_buf + offset,
                           remain,
                           "%s%u",
                           (i == 0U) ? "" : ",",
                           (unsigned int)valid);
        if ((written < 0) || ((size_t)written >= remain))
        {
            return;
        }
        offset += (size_t)written;
        remain -= (size_t)written;
    }

    written = snprintf(tx_buf + offset, remain, "\n");
    if ((written < 0) || ((size_t)written >= remain))
    {
        return;
    }
    offset += (size_t)written;

    (void)HAL_UART_Transmit(bsp_get_huart(USART6_INSTANCE), (uint8_t *)tx_buf, (uint16_t)offset, 20U);
    s_linux_tx_count++;
    if ((s_linux_tx_count % 30U) == 0U)
    {
        log_i("tof usart6 tx frame_id=%lu tx_count=%lu", (unsigned long)s_frame_id, (unsigned long)s_linux_tx_count);
    }
}

void vl53l8cx_app_thread_init(void)
{
    UINT tx_status;
    s_vl53l8cx_debug_stage = 100U;

    tx_status = tx_thread_create(&s_vl53l8cx_thread,
                                 "vl53l8cx",
                                 thread_vl53l8cx_entry,
                                 0,
                                 s_vl53l8cx_stack,
                                 sizeof(s_vl53l8cx_stack),
                                 VL53L8CX_APP_THREAD_PRIORITY,
                                 VL53L8CX_APP_THREAD_PRIORITY,
                                 TX_NO_TIME_SLICE,
                                 TX_AUTO_START);
    s_vl53l8cx_debug_last_status = (uint32_t)tx_status;
    if (tx_status != TX_SUCCESS)
    {
        s_vl53l8cx_debug_stage = 101U;
        log_w("vl53l8cx thread create failed, st=%u", (unsigned int)tx_status);
    }
    else
    {
        s_vl53l8cx_debug_stage = 102U;
        log_i("vl53l8cx thread create ok");
    }
}

uint8_t vl53l8cx_app_is_ready(void)
{
    return s_ready;
}

uint8_t vl53l8cx_app_get_latest(uint16_t *distance_mm, uint8_t max_count, uint8_t *valid_count)
{
    uint8_t i;
    uint8_t count;

    if ((distance_mm == 0) || (valid_count == 0) || (s_has_frame == 0U))
    {
        return 0U;
    }

    count = (uint8_t)VL53L8CX_APP_RESOLUTION;
    if (count > max_count)
    {
        count = max_count;
    }

    for (i = 0U; i < count; i++)
    {
        distance_mm[i] = (uint16_t)s_results.distance_mm[i];
    }

    *valid_count = count;
    return 1U;
}

uint8_t vl53l8cx_app_get_frame(vl53l8cx_app_frame_t *frame)
{
    uint8_t i;

    if ((frame == 0) || (s_has_frame == 0U))
    {
        return 0U;
    }

    memset(frame, 0, sizeof(*frame));
    frame->valid = 1U;
    frame->resolution = VL53L8CX_APP_RESOLUTION;
    frame->silicon_temp_degc = s_results.silicon_temp_degc;

    for (i = 0U; i < (uint8_t)VL53L8CX_APP_ZONE_COUNT; i++)
    {
        frame->distance_mm[i] = (uint16_t)s_results.distance_mm[i];
        frame->target_status[i] = s_results.target_status[i];
        frame->signal_per_spad[i] = s_results.signal_per_spad[i];
        frame->ambient_per_spad[i] = s_results.ambient_per_spad[i];
    }

    return 1U;
}
