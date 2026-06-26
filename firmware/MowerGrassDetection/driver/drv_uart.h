#pragma once

#include "stm32f4xx_hal.h"
#include "tx_api.h"

#include "../src/utils.h"
#include "app_threadx.h"
#include "../libraries/lwrb/lwrb/src/include/lwrb/lwrb.h"

typedef void (*uart_rx_handler_cb)(const uint8_t data);

typedef struct
{
    TX_THREAD thread;
    uint32_t  stack_size;
    uint8_t   priority;
} uart_thread_t;

#define UART_THREAD_INIT(_stack_size, _priority) {.stack_size = _stack_size, .priority = _priority}

typedef struct
{
    UART_HandleTypeDef   huart;
    USART_TypeDef *const uart;
    uint8_t             *pool_buff;
    uart_thread_t        rx_thread;

    TX_BYTE_POOL pool;
    TX_SEMAPHORE rx_sem;

    uint8_t       *rx_buff;
    const uint32_t pool_size;
    const uint32_t tx_buff_size;
    const uint32_t rx_buff_size;

    lwrb_t   lwrb;
    uint32_t usart_tx_len;
    uint32_t usart_tx_over_tick;

    uint32_t pos;

    uart_rx_handler_cb rx_cb;
    bool               init_flag;
} uarts_handle_t;

#define UART_POOL_USE_MEMORY_SIZE (100) // byte pool used 35 byte

/**
 * @brief UART在系统资源分配时出错处理
 */
#define UART_SYS_INIT_ERR_HANDLE(ret) \
    if (ret != TX_SUCCESS)            \
        while (true)                  \
        {                             \
        };

#define UART_HANDLE_INIT(_uart, _pool_buff, _tx_buff_size, _rx_buff_size, rx_stack_size, rx_priority) \
    {                                                                                                 \
        .uart         = _uart,                                                                        \
        .pool_buff    = _pool_buff,                                                                   \
        .rx_buff_size = _rx_buff_size,                                                                \
        .tx_buff_size = _tx_buff_size,                                                                \
        .pool_size    = _tx_buff_size + _rx_buff_size + rx_stack_size + UART_POOL_USE_MEMORY_SIZE,    \
        .rx_thread    = UART_THREAD_INIT(rx_stack_size, rx_priority),                                 \
    }

#define UART_HANDLE_CREATE(uart, tx_buff_size, rx_buff_size, rx_stack_size, rx_priority)                     \
    uint8_t uart##_pool_buff[tx_buff_size + rx_buff_size + rx_stack_size + UART_POOL_USE_MEMORY_SIZE] = {0}; \
                                                                                                             \
    uarts_handle_t uart##_handle                                                                             \
        = UART_HANDLE_INIT(uart, uart##_pool_buff, tx_buff_size, rx_buff_size, rx_stack_size, rx_priority);

typedef enum
{
    USART1_INSTANCE = 0,
    USART2_INSTANCE,
    USART6_INSTANCE,

    UART_CHANNEL_NUM,
} uart_index_e;

UART_HandleTypeDef *bsp_get_huart(uart_index_e index);

void HAL_UART_INTERRUPT_HANDLE(UART_HandleTypeDef *huart);

/**
 * @brief 串口初始化
 *
 * @param index     串口索引号, 严格按照枚举使用
 * @param baudrate  波特率
 * @param word_len  位长度, UART_WORDLENGTH_8B, UART_WORDLENGTH_9B
 * @param stop_bits 停止位, UART_STOPBITS_1, UART_STOPBITS_2
 * @param parity    校验位, UART_PARITY_NONE, UART_PARITY_EVEN, UART_PARITY_ODD
 */
void bsp_uart_init(uart_index_e index, uint32_t baudrate, uint32_t word_len, uint32_t stop_bits, uint32_t parity);
void bsp_uart_install_rx_callback(uart_index_e index, uart_rx_handler_cb cb);

/**
 * @brief 串口发送数据
 *
 * @param index 串口索引号, 严格按照枚举使用
 * @param data  发送的数据
 * @param len   数据长度
 */
void bsp_uart_send(uart_index_e index, uint8_t *data, uint16_t len);
