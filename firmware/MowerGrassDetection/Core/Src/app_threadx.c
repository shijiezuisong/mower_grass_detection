/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"
#include "tx_initialize.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern volatile ULONG  _tx_thread_system_state;
extern volatile uint32_t uwTick;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;

  /* USER CODE BEGIN App_ThreadX_Init */
  /* USER CODE END App_ThreadX_Init */

  return ret;
}

/**
  * @brief  MX_ThreadX_Init
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN  Before_Kernel_Start */

  /* USER CODE END  Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN  Kernel_Start_Error */

  /* USER CODE END  Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */
void sys_delay_ms(uint32_t nMs)
{
    ULONG ticks = (nMs * TX_TIMER_TICKS_PER_SECOND + 999U) / 1000U;

    if ((nMs > 0U) && (ticks == 0U))
    {
        ticks = 1U;
    }

    tx_thread_sleep(ticks);
}

uint32_t sys_get_ms(void)
{
    return (uint32_t)((tx_time_get() * 1000U) / TX_TIMER_TICKS_PER_SECOND);
}

uint32_t HAL_GetTick(void)
{
  if (tx_thread_identify() != TX_NULL)
  {
    return sys_get_ms();
  }

  if ((_tx_thread_system_state != TX_INITIALIZE_IN_PROGRESS)
      && (_tx_thread_system_state != TX_INITIALIZE_ALMOST_DONE)
      && (_tx_thread_system_state != TX_INITIALIZE_IS_FINISHED))
  {
    return sys_get_ms();
  }

  return uwTick;
}
/* USER CODE END 1 */
