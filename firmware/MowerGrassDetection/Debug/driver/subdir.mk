################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../driver/drv_uart.c 

OBJS += \
./driver/drv_uart.o 

C_DEPS += \
./driver/drv_uart.d 


# Each subdirectory must supply rules for building sources it contributes
driver/%.o driver/%.su driver/%.cyclo: ../driver/%.c driver/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DTX_INCLUDE_USER_DEFINE_FILE -DUSE_HAL_DRIVER -DSTM32F401xE -c -I../Core/Inc -I../AZURE_RTOS/App -I../X-CUBE-MEMS1/Target -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Drivers/BSP/Components/lsm6dso -I../Middlewares/ST/threadx/common/inc/ -I../Middlewares/ST/threadx/ports/cortex_m4/gnu/inc/ -I../libraries/easylogger -I../libraries/lwrb/lwrb/src/include -O2 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-driver

clean-driver:
	-$(RM) ./driver/drv_uart.cyclo ./driver/drv_uart.d ./driver/drv_uart.o ./driver/drv_uart.su

.PHONY: clean-driver

