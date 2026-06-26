#include "../driver/drv_uart.h"

#include "log.h"
#include "../libraries/easylogger/elog.h"

#include "app_threadx.h"

static void elog_init_params(void)
{
    /** 初始化 EASYLOG 组件 */
    elog_init();

    /** 日志格式及样式 */
    /** 断言: 输出所有内容 */
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_TIME | ELOG_FMT_P_INFO);
    /** 错误: 输出级别, 标签 */
    elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_TIME | ELOG_FMT_P_INFO);
    /** 警告: 输出级别, 标签 */
    elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_TIME | ELOG_FMT_P_INFO);
    /** 信息: 输出级别, 标签 */
    elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_TIME | ELOG_FMT_P_INFO);
    /** 调试: 输出除了方法名之外的所有内容 */
    elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_TIME | ELOG_FMT_P_INFO);
    /** 详细: 输出除了方法名之外的所有内容 */
    elog_set_fmt(ELOG_LVL_VERBOSE, ELOG_FMT_TIME | ELOG_FMT_P_INFO);

    /** 启动日志输出 */
    elog_start();
}

void log_init(void)
{
    bsp_uart_init(USART2_INSTANCE, 921600, UART_WORDLENGTH_8B, UART_STOPBITS_1, UART_PARITY_NONE);
    /** 初始化 EASYLOG 组件 */
    elog_init_params();
}

uint8_t test_cnt;

static void thread_log_entry(ULONG thread_input)
{
    while (1)
    {
        if (test_cnt < 200)
        {
            test_cnt++;
        }
        else
        {
            test_cnt = 0;
        }

        log_i("thread log entry");
        sys_delay_ms(100);
    }
}

void blackbox_init(void)
{
    static TX_THREAD _thread;
    static uint8_t   _thread_stack[1024 * 1];

    tx_thread_create(&_thread,
                     "logger",
                     thread_log_entry,
                     0,
                     &_thread_stack[0],
                     sizeof(_thread_stack),
                     TX_THREAD_PRIORITY_UART_RX_LOG_DEBUG,
                     TX_THREAD_PRIORITY_UART_RX_LOG_DEBUG,
                     TX_NO_TIME_SLICE,
                     TX_AUTO_START);
}
