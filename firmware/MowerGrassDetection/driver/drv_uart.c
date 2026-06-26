#include "drv_uart.h"

#define UART_TX_WAIT_TIME_MS 5000

// uart, tx_buff_size, rx_buff_size, rx_stack_size, rx_priority

UART_HANDLE_CREATE(USART1, 2048, 1024, 2048, TX_THREAD_PRIORITY_UART_RX_MOWER); // Mower
UART_HANDLE_CREATE(USART2, 2048, 1024, 2048, TX_THREAD_PRIORITY_UART_RX_LOG);   // Debug
UART_HANDLE_CREATE(USART6, 2048, 1024, 2048, TX_THREAD_PRIORITY_UART_RX_ALGO);  // Linux

uarts_handle_t *const uart_handle_array[] = {
    &USART1_handle,
    &USART2_handle,
    &USART6_handle,
};

static void usart_process_data(uarts_handle_t *uart_handle, const uint8_t *data, size_t len)
{
    if (uart_handle->rx_cb)
    {
        const uint8_t *d = data;

        for (; len > 0; --len, ++d)
        {
            uart_handle->rx_cb(*d);
        }
    }
}

static void usart_rx_check(uarts_handle_t *uart_handle)
{
    /* Calculate current position in buffer */
    const size_t pos = uart_handle->rx_buff_size - __HAL_DMA_GET_COUNTER(uart_handle->huart.hdmarx);

    if (pos != uart_handle->pos)
    {
        /* Check change in received data */
        if (pos > uart_handle->pos)
        {
            /*
             * Processing is done in "linear" mode.
             *
             * Application processing is fast with single data block,
             * length is simply calculated by subtracting pointers
             *
             * [   0   ]
             * [   1   ] <- old_pos |------------------------------------|
             * [   2   ]            |                                    |
             * [   3   ]            | Single block (len = pos - old_pos) |
             * [   4   ]            |                                    |
             * [   5   ]            |------------------------------------|
             * [   6   ] <- pos
             * [   7   ]
             * [ N - 1 ]
             */
            usart_process_data(uart_handle, &uart_handle->rx_buff[uart_handle->pos], pos - uart_handle->pos);
        }
        else
        {
            /*
             * Processing is done in "overflow" mode..
             *
             * Application must process data twice,
             * since there are 2 linear memory blocks to handle
             *
             * [   0   ]            |---------------------------------|
             * [   1   ]            | Second block (len = pos)        |
             * [   2   ]            |---------------------------------|
             * [   3   ] <- pos
             * [   4   ] <- old_pos |---------------------------------|
             * [   5   ]            |                                 |
             * [   6   ]            | First block (len = N - old_pos) |
             * [   7   ]            |                                 |
             * [ N - 1 ]            |---------------------------------|
             */
            usart_process_data(uart_handle,
                               &uart_handle->rx_buff[uart_handle->pos],
                               uart_handle->rx_buff_size - uart_handle->pos);

            /* Check and continue with beginning of buffer */
            if (pos > 0)
            {
                usart_process_data(uart_handle, &uart_handle->rx_buff[0], pos);
            }
        }
    }

    uart_handle->pos = pos; /* Save current position as old */
}

static int usart_tx_start(UART_HandleTypeDef *huart, void *data, uint16_t size)
{
    /** 考虑无 DMA 发送的情况 */
    if (huart->hdmatx == NULL)
    {
        return HAL_UART_Transmit(huart, data, size, UART_TX_WAIT_TIME_MS);
    }
    else
    {
        /* Clear all flags */
        __HAL_DMA_CLEAR_FLAG(huart->hdmatx, __HAL_DMA_GET_TC_FLAG_INDEX(huart));
        __HAL_DMA_CLEAR_FLAG(huart->hdmatx, __HAL_DMA_GET_HT_FLAG_INDEX(huart));
        __HAL_DMA_CLEAR_FLAG(huart->hdmatx, __HAL_DMA_GET_DME_FLAG_INDEX(huart));
        __HAL_DMA_CLEAR_FLAG(huart->hdmatx, __HAL_DMA_GET_FE_FLAG_INDEX(huart));
        __HAL_DMA_CLEAR_FLAG(huart->hdmatx, __HAL_DMA_GET_TE_FLAG_INDEX(huart));

        /* Configure DMA */
        if (HAL_OK == HAL_UART_Transmit_DMA(huart, data, size))
        {
            return HAL_OK;
        }
    }

    return HAL_ERROR;
}

static void uart_rx_thread_entry(ULONG thread_input)
{
    uarts_handle_t *const uart_handle = (uarts_handle_t *)thread_input;

    /* 创建信号量, 初始值是 0 */
    tx_semaphore_create(&uart_handle->rx_sem, "uart_rx", 0);

    while (1)
    {
        /* 永久方式等待信号量 */
        const uint8_t result = tx_semaphore_get(&uart_handle->rx_sem, TX_WAIT_FOREVER);

        if (result == TX_SUCCESS)
        {
            usart_rx_check(uart_handle);
        }

        HAL_UART_Receive_DMA(&uart_handle->huart, uart_handle->rx_buff, uart_handle->rx_buff_size);
    }
}

static void uart_config(uarts_handle_t *uart_handle, uint32_t baudrate, uint32_t word_len, uint32_t stop_bits, uint32_t parity)
{
    uart_handle->huart.Instance          = uart_handle->uart;
    uart_handle->huart.Init.BaudRate     = baudrate;
    uart_handle->huart.Init.WordLength   = word_len;
    uart_handle->huart.Init.StopBits     = stop_bits;
    uart_handle->huart.Init.Parity       = parity;
    uart_handle->huart.Init.Mode         = UART_MODE_TX_RX;
    uart_handle->huart.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    uart_handle->huart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&uart_handle->huart);

    /* Enable DMA tx complete interupt */
    if (uart_handle->huart.hdmatx)
    {
        __HAL_DMA_ENABLE_IT(uart_handle->huart.hdmatx, DMA_IT_TC);
    }

    /* Enable idle line interrupt */
    __HAL_UART_ENABLE_IT(&uart_handle->huart, UART_IT_IDLE);

    /* Enable DMA rx HT & TC interrupts */
    if (uart_handle->huart.hdmarx)
    {
        __HAL_DMA_ENABLE_IT(uart_handle->huart.hdmarx, DMA_IT_TC);
        __HAL_DMA_ENABLE_IT(uart_handle->huart.hdmarx, DMA_IT_HT);

        /* Start receiving. */
        HAL_UART_Receive_DMA(&uart_handle->huart, uart_handle->rx_buff, uart_handle->rx_buff_size);
    }
}

static void bsp_uart_sys_init(uarts_handle_t *uart_handle)
{
    if (uart_handle->init_flag)
    {
        return;
    }

    UINT tx_status = TX_SUCCESS;

    TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL *)&uart_handle->pool;
    uint8_t      *pointer   = NULL;

    /* 分配UART所需内存资源 */
    tx_status = tx_byte_pool_create(byte_pool, "uart pool", uart_handle->pool_buff, uart_handle->pool_size);
    UART_SYS_INIT_ERR_HANDLE(tx_status);

    /* 串口接收完成信号量  */
    tx_semaphore_create(&uart_handle->rx_sem, "uart rx dma cplt", 0);

    /* 分配UART RX缓冲区 */
    tx_status = tx_byte_allocate(byte_pool, (VOID **)&uart_handle->rx_buff, uart_handle->rx_buff_size, TX_NO_WAIT);
    UART_SYS_INIT_ERR_HANDLE(tx_status);

    /* UART 接收线程创建 */
    tx_status = tx_byte_allocate(byte_pool, (VOID **)&pointer, uart_handle->rx_thread.stack_size, TX_NO_WAIT);
    UART_SYS_INIT_ERR_HANDLE(tx_status);

    tx_status = tx_thread_create(&uart_handle->rx_thread.thread,
                                 "uart rx",
                                 uart_rx_thread_entry,
                                 (ULONG)uart_handle,
                                 pointer,
                                 uart_handle->rx_thread.stack_size,
                                 uart_handle->rx_thread.priority,
                                 uart_handle->rx_thread.priority,
                                 100,
                                 TX_AUTO_START);
    UART_SYS_INIT_ERR_HANDLE(tx_status);

    tx_status = tx_byte_allocate(byte_pool, (VOID **)&pointer, uart_handle->tx_buff_size, TX_NO_WAIT);
    UART_SYS_INIT_ERR_HANDLE(tx_status);

    /* 初始化发送环形缓冲区 */
    lwrb_init(&uart_handle->lwrb, pointer, uart_handle->tx_buff_size);

    uart_handle->init_flag = true;
}

void bsp_uart_init(uart_index_e index, uint32_t baudrate, uint32_t word_len, uint32_t stop_bits, uint32_t parity)
{
    uarts_handle_t *uart_handle = uart_handle_array[index];

    /* 串口任务资源分配 */
    bsp_uart_sys_init(uart_handle);

    /* 初始化串口外设 */
    uart_config(uart_handle, baudrate, word_len, stop_bits, parity);
}

static void usart_start_tx_dma_transfer(uarts_handle_t *uart_handle)
{
    /*
     * First check if transfer is currently in-active,
     * by examining the value of usart_tx_dma_current_len variable.
     *
     * This variable is set before DMA transfer is started and cleared in DMA TX complete interrupt.
     *
     * It is not necessary to disable the interrupts before checking the variable:
     *
     * When usart_tx_dma_current_len == 0
     *    - This function is called by either application or TX DMA interrupt
     *    - When called from interrupt, it was just reset before the call,
     *         indicating transfer just completed and ready for more
     *    - When called from an application, transfer was previously already in-active
     *         and immediate call from interrupt cannot happen at this moment
     *
     * When usart_tx_dma_current_len != 0
     *    - This function is called only by an application.
     *    - It will never be called from interrupt with usart_tx_dma_current_len != 0 condition
     *
     * Disabling interrupts before checking for next transfer is advised
     * only if multiple operating system threads can access to this function w/o
     * exclusive access protection (mutex) configured,
     * or if application calls this function from multiple interrupts.
     *
     * This example assumes worst use case scenario,
     * hence interrupts are disabled prior every check
     */

    uint32_t len = 0;

    while ((len = lwrb_get_linear_block_read_length(&uart_handle->lwrb)) > 0)
    {
        if (uart_handle->usart_tx_len > 0)
        {
            /** 串口发送超时5S, 重新发送 */
            if (tx_time_get() < uart_handle->usart_tx_over_tick + UART_TX_WAIT_TIME_MS)
            {
                break;
            }
        }

        uart_handle->usart_tx_over_tick = tx_time_get();

        /** 发送数据 */
        int ret = usart_tx_start(&uart_handle->huart, (uint8_t *)lwrb_get_linear_block_read_address(&uart_handle->lwrb), len);
        uart_handle->usart_tx_len = len;
        if (ret != HAL_OK)
        {
            /** 释放内存占用 */
            lwrb_skip(&uart_handle->lwrb, uart_handle->usart_tx_len);
            uart_handle->usart_tx_len = 0;
        }

        if (uart_handle->huart.hdmatx == NULL)
        {
            /** 释放内存占用 */
            lwrb_skip(&uart_handle->lwrb, uart_handle->usart_tx_len);
            uart_handle->usart_tx_len = 0;
        }
    }
}

void bsp_uart_send(uart_index_e index, uint8_t *data, uint16_t len)
{
    uarts_handle_t *uart_handle = uart_handle_array[index];
    if (lwrb_get_free(&uart_handle->lwrb) >= len)
    {
        lwrb_write(&uart_handle->lwrb, data, len);

        /** 发起串口数据写入 */
        usart_start_tx_dma_transfer(uart_handle);
    }
}

UART_HandleTypeDef *bsp_get_huart(uart_index_e index)
{
    return &uart_handle_array[index]->huart;
}

void bsp_uart_install_rx_callback(uart_index_e index, uart_rx_handler_cb cb)
{
    uart_handle_array[index]->rx_cb = cb;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < ARRAYLEN(uart_handle_array); i++)
    {
        if (uart_handle_array[i]->huart.Instance == huart->Instance)
        {
            /** 该部分的数据已经发送完成, 要跳过 */
            lwrb_skip(&uart_handle_array[i]->lwrb, uart_handle_array[i]->usart_tx_len);
            uart_handle_array[i]->usart_tx_len = 0;

            /** 继续发送缓冲区剩下的字节 */
            usart_start_tx_dma_transfer(uart_handle_array[i]);
            return;
        }
    }
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < ARRAYLEN(uart_handle_array); i++)
    {
        if (uart_handle_array[i]->huart.Instance == huart->Instance)
        {
            tx_semaphore_put(&uart_handle_array[i]->rx_sem);
            return;
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < ARRAYLEN(uart_handle_array); i++)
    {
        if (uart_handle_array[i]->huart.Instance == huart->Instance)
        {
            tx_semaphore_put(&uart_handle_array[i]->rx_sem);
            return;
        }
    }
}

void HAL_UART_INTERRUPT_HANDLE(UART_HandleTypeDef *huart)
{
    /** Interrupt handling. */
    HAL_UART_IRQHandler(huart);

    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE) != RESET)
    {
        /** Clear idle line interrupt flag. */
        __HAL_UART_CLEAR_IDLEFLAG(huart);

        /** 闲时函数回调 */
        for (uint8_t i = 0; i < ARRAYLEN(uart_handle_array); i++)
        {
            if (uart_handle_array[i]->huart.Instance == huart->Instance)
            {
                tx_semaphore_put(&uart_handle_array[i]->rx_sem);
                return;
            }
        }
    }
}