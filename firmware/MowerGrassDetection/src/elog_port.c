#include <stdio.h>

#ifdef USING_SEGGER_RTT_DEBUG
#include "SEGGER_RTT.h"
#include "SEGGER_RTT_Conf.h"
#endif

#include "tx_api.h"
#include "tx_thread.h"

#include "../libraries/easylogger/elog.h"
#include "../driver/drv_uart.h"

static TX_MUTEX mutex_elog_out;
static UINT     s_elog_mutex_ready;

static void elog_port_try_create_mutex(void)
{
    if (s_elog_mutex_ready != 0U)
    {
        return;
    }

    if (TX_THREAD_GET_SYSTEM_STATE() != 0U)
    {
        return;
    }

    if (tx_mutex_create(&mutex_elog_out, "mutex elog out", TX_NO_INHERIT) == TX_SUCCESS)
    {
        s_elog_mutex_ready = 1U;
    }
}
// static char     tmp_str[35] = {0};

/**
 * EasyLogger port initialize
 *
 * @return result
 */
ElogErrCode elog_port_init(void)
{
    s_elog_mutex_ready = 0U;
    elog_port_try_create_mutex();

#ifdef USING_SEGGER_RTT_DEBUG
    /** Init RTT Tools */
    SEGGER_RTT_Init();

    /** 配置 RTT 输出通道 (在非阻塞模式下 ,只有不超过缓冲区大小的数据被写入, 其余的数据将被丢弃) */
    SEGGER_RTT_ConfigUpBuffer(0, "RTTUP", NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    SEGGER_RTT_ConfigDownBuffer(0, "RTTDOWN", NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
#endif

    return ELOG_NO_ERR;
}

/**
 * EasyLogger port deinitialize
 */
void elog_port_deinit(void)
{
}

/**
 * output log port interface
 *
 * @param log output of log
 * @param size log size
 */
void elog_port_output(const char *log, size_t size)
{
#ifdef USING_SEGGER_RTT_DEBUG
    /** output to terminal */
    SEGGER_RTT_Write(0, log, size);
#endif

    bsp_uart_send(USART2_INSTANCE, (uint8_t *)log, size);
}

/**
 * output lock
 */
void elog_port_output_lock(void)
{
    elog_port_try_create_mutex();

    if ((s_elog_mutex_ready != 0U) && (TX_THREAD_GET_SYSTEM_STATE() == 0U))
    {
        (void)tx_mutex_get(&mutex_elog_out, TX_WAIT_FOREVER);
    }
}

/**
 * output unlock
 */
void elog_port_output_unlock(void)
{
    if ((s_elog_mutex_ready != 0U) && (TX_THREAD_GET_SYSTEM_STATE() == 0U))
    {
        (void)tx_mutex_put(&mutex_elog_out);
    }
}

/**
 * get current time interface 获取当前时间
 *
 * @return current time
 */
const char *elog_port_get_time(void)
{
    static char time_buf[20];

    /** 获取系统运行时间（秒）（基于 SysTick） */
    unsigned long uptime_sec = sys_get_ms() / 1000;
    snprintf(time_buf, sizeof(time_buf), "T+%lus", uptime_sec);
    return time_buf;
    // memset(tmp_str, 0, sizeof(tmp_str));

    // static rtc_data_t rtc_record = {0};
    // rtc_data_t        data;
    // rtc_get_date_time(&data);
    // uint8_t index = 0;

    // if (rtc_record.day != data.day || rtc_record.month != data.month || rtc_record.year != data.year)
    // {
    //     rtc_record.year = data.year;
    //     sprintf(tmp_str, "%d-", data.year);
    //     index += 5;

    //     rtc_record.month = data.month;
    //     sprintf(tmp_str + index, "%02d-", data.month);
    //     index += 3;

    //     rtc_record.day = data.day;
    //     sprintf(tmp_str + index, "%02d ", data.day);
    //     index += 3;
    // }

    // rtc_record.hours = data.hours;
    // sprintf(tmp_str + index, "%02d:", data.hours);
    // index += 3;

    // rtc_record.minutes = data.minutes;
    // sprintf(tmp_str + index, "%02d:", data.minutes);
    // index += 3;

    // sprintf(tmp_str + index, "%02d:", data.seconds);
    // index += 3;

    // sprintf(tmp_str + index, "%03d", (int)(sys_get_ms() % 1000));
    // return tmp_str;
}

/**
 * get current process name interface 获取进程名称
 *
 * @return current process name
 */

const char *elog_port_get_p_info(void)
{
    return "FW:v0.0.1";
    // memset(tmp_str, 0, sizeof(tmp_str));

    // static int index = 0;
    // sprintf(tmp_str, "#%06d", index++);
    // return tmp_str;
}

/**
 * get current thread name interface 获取线程名称
 *
 * @return current thread name
 */
const char *elog_port_get_t_info(void)
{
    TX_THREAD *a;
    TX_THREAD_GET_CURRENT(a);

    return a->tx_thread_name;
}
