################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../Core/Startup/startup_stm32f401retx.s 

OBJS += \
./Core/Startup/startup_stm32f401retx.o 

S_DEPS += \
./Core/Startup/startup_stm32f401retx.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Startup/%.o: ../Core/Startup/%.s Core/Startup/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m4 -g3 -DDEBUG -c -I../Core/Inc -I"E:/WorkSpace/TTI/PRP/mower_grass_detection/firmware/libraries/lwrb/lwrb/src/include" -I"E:/WorkSpace/TTI/PRP/mower_grass_detection/firmware/MowerGrassDetection/driver" -I"E:/WorkSpace/TTI/PRP/mower_grass_detection/firmware/MowerGrassDetection/libraries/easylogger" -I"E:/WorkSpace/TTI/PRP/mower_grass_detection/firmware/MowerGrassDetection/libraries/lwrb" -I"E:/WorkSpace/TTI/PRP/mower_grass_detection/firmware/MowerGrassDetection/libraries/lwrb/lwrb/src/include" -I"E:/WorkSpace/TTI/PRP/mower_grass_detection/firmware/MowerGrassDetection/src" -I../AZURE_RTOS/App -I../X-CUBE-MEMS1/Target -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Drivers/BSP/Components/lsm6dso -I../Middlewares/ST/threadx/common/inc/ -I../Middlewares/ST/threadx/ports/cortex_m4/gnu/inc/ -I../libraries/easylogger -I../libraries/lwrb/lwrb/src/include -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"

clean: clean-Core-2f-Startup

clean-Core-2f-Startup:
	-$(RM) ./Core/Startup/startup_stm32f401retx.d ./Core/Startup/startup_stm32f401retx.o

.PHONY: clean-Core-2f-Startup

