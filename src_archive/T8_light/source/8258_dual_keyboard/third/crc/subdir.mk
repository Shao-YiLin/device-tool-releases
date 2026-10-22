################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../third/crc/mblue_crc.c 

OBJS += \
./third/crc/mblue_crc.o 


# Each subdirectory must supply rules for building sources it contributes
third/crc/%.o: ../third/crc/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I"E:\fw_workspace\trunk\source\telink\keyboard\T8_light\source" -D__PROJECT_825X_DUAL_KEYBOARD__=1 -DCHIP_TYPE=CHIP_TYPE_8258 -DUART_PRINT=0 -DSIGNATURE=0 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


