################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../libraries/lwrb/lwrb/src/lwrb/lwrb.c \
../libraries/lwrb/lwrb/src/lwrb/lwrb_ex.c 

OBJS += \
./libraries/lwrb/lwrb/src/lwrb/lwrb.o \
./libraries/lwrb/lwrb/src/lwrb/lwrb_ex.o 

C_DEPS += \
./libraries/lwrb/lwrb/src/lwrb/lwrb.d \
./libraries/lwrb/lwrb/src/lwrb/lwrb_ex.d 


# Each subdirectory must supply rules for building sources it contributes
libraries/lwrb/lwrb/src/lwrb/%.o libraries/lwrb/lwrb/src/lwrb/%.su libraries/lwrb/lwrb/src/lwrb/%.cyclo: ../libraries/lwrb/lwrb/src/lwrb/%.c libraries/lwrb/lwrb/src/lwrb/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DTX_INCLUDE_USER_DEFINE_FILE -DUSE_HAL_DRIVER -DSTM32F401xE -c -I../Core/Inc -I../AZURE_RTOS/App -I../X-CUBE-MEMS1/Target -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Drivers/BSP/Components/lsm6dso -I../Middlewares/ST/threadx/common/inc/ -I../Middlewares/ST/threadx/ports/cortex_m4/gnu/inc/ -I../libraries/easylogger -I../libraries/lwrb/lwrb/src/include -O2 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-libraries-2f-lwrb-2f-lwrb-2f-src-2f-lwrb

clean-libraries-2f-lwrb-2f-lwrb-2f-src-2f-lwrb:
	-$(RM) ./libraries/lwrb/lwrb/src/lwrb/lwrb.cyclo ./libraries/lwrb/lwrb/src/lwrb/lwrb.d ./libraries/lwrb/lwrb/src/lwrb/lwrb.o ./libraries/lwrb/lwrb/src/lwrb/lwrb.su ./libraries/lwrb/lwrb/src/lwrb/lwrb_ex.cyclo ./libraries/lwrb/lwrb/src/lwrb/lwrb_ex.d ./libraries/lwrb/lwrb/src/lwrb/lwrb_ex.o ./libraries/lwrb/lwrb/src/lwrb/lwrb_ex.su

.PHONY: clean-libraries-2f-lwrb-2f-lwrb-2f-src-2f-lwrb

