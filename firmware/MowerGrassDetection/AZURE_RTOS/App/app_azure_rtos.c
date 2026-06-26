
/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    app_azure_rtos.c
 * @author  MCD Application Team
 * @brief   azure_rtos application implementation file
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

#include "app_azure_rtos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../../src/log.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MEM_HEAP_SIZE_KB 20
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static TX_BYTE_POOL mem_byte_pool;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
 * @brief  Define the initial system.
 * @param  first_unused_memory : Pointer to the first unused memory
 * @retval None
 */
VOID tx_application_define(VOID *first_unused_memory)
{
    /*
     * Using dynamic memory allocation requires to apply some changes to the linker file.
     * ThreadX needs to pass a pointer to the first free memory location in RAM to the tx_application_define() function,
     * using the "first_unused_memory" argument.
     * This require changes in the linker files to expose this memory location.
     * For EWARM add the following section into the .icf file:
         place in RAM_region    { last section FREE_MEM };
     * For MDK-ARM
         - either define the RW_IRAM1 region in the ".sct" file
         - or modify the line below in "tx_low_level_initilize.s to match the memory region being used
            LDR r1, =|Image$$RW_IRAM1$$ZI$$Limit|

     * For STM32CubeIDE add the following section into the .ld file:
         ._threadx_heap :
           {
              . = ALIGN(8);
              __RAM_segment_used_end__ = .;
              . = . + 64K;
              . = ALIGN(8);
            } >RAM_D1 AT> RAM_D1
        * The simplest way to provide memory for ThreadX is to define a new section, see ._threadx_heap above.
        * In the example above the ThreadX heap size is set to 64KBytes.
        * The ._threadx_heap must be located between the .bss and the ._user_heap_stack sections in the linker script.
        * Caution: Make sure that ThreadX does not need more than the provided heap memory (64KBytes in this example).
        * Read more in STM32CubeIDE User Guide, chapter: "Linker script".

     * The "tx_initialize_low_level.s" should be also modified to enable the "USE_DYNAMIC_MEMORY_ALLOCATION" flag.
     */

    /* USER CODE BEGIN DYNAMIC_MEM_ALLOC */
    (void)first_unused_memory;

    /* USER CODE BEGIN  tx_application_define */
    static uint8_t _heap[1024 * MEM_HEAP_SIZE_KB];
    tx_byte_pool_create(&mem_byte_pool, "mem_byte_pool", &_heap, sizeof(_heap));

  blackbox_init();
    /* USER CODE END  tx_application_define */

    /* USER CODE END DYNAMIC_MEM_ALLOC */
}

/* USER CODE BEGIN  0 */

/* USER CODE END  0 */
