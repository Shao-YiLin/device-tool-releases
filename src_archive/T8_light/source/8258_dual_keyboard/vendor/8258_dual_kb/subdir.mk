################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../vendor/8258_dual_kb/AAA_LED.c \
../vendor/8258_dual_kb/AAA_Public.c \
../vendor/8258_dual_kb/AAA_USB_app.c \
../vendor/8258_dual_kb/AAA_app.c \
../vendor/8258_dual_kb/AAA_app_att.c \
../vendor/8258_dual_kb/AAA_battery_check.c \
../vendor/8258_dual_kb/AAA_main.c \
../vendor/8258_dual_kb/OTA_manager.c \
../vendor/8258_dual_kb/USB_manager.c \
../vendor/8258_dual_kb/apple_mac.c \
../vendor/8258_dual_kb/audio_manager.c \
../vendor/8258_dual_kb/battery_manager.c \
../vendor/8258_dual_kb/custom_button_logic_manager.c \
../vendor/8258_dual_kb/data_out_manager.c \
../vendor/8258_dual_kb/keyboard_manager.c \
../vendor/8258_dual_kb/keyboard_scan.c \
../vendor/8258_dual_kb/led_manager.c \
../vendor/8258_dual_kb/logic_protocol_manager.c \
../vendor/8258_dual_kb/mblue_identification_service.c \
../vendor/8258_dual_kb/pair_info_manager.c \
../vendor/8258_dual_kb/pair_logic_manager.c \
../vendor/8258_dual_kb/sonix_keyboard_manager.c 

OBJS += \
./vendor/8258_dual_kb/AAA_LED.o \
./vendor/8258_dual_kb/AAA_Public.o \
./vendor/8258_dual_kb/AAA_USB_app.o \
./vendor/8258_dual_kb/AAA_app.o \
./vendor/8258_dual_kb/AAA_app_att.o \
./vendor/8258_dual_kb/AAA_battery_check.o \
./vendor/8258_dual_kb/AAA_main.o \
./vendor/8258_dual_kb/OTA_manager.o \
./vendor/8258_dual_kb/USB_manager.o \
./vendor/8258_dual_kb/apple_mac.o \
./vendor/8258_dual_kb/audio_manager.o \
./vendor/8258_dual_kb/battery_manager.o \
./vendor/8258_dual_kb/custom_button_logic_manager.o \
./vendor/8258_dual_kb/data_out_manager.o \
./vendor/8258_dual_kb/keyboard_manager.o \
./vendor/8258_dual_kb/keyboard_scan.o \
./vendor/8258_dual_kb/led_manager.o \
./vendor/8258_dual_kb/logic_protocol_manager.o \
./vendor/8258_dual_kb/mblue_identification_service.o \
./vendor/8258_dual_kb/pair_info_manager.o \
./vendor/8258_dual_kb/pair_logic_manager.o \
./vendor/8258_dual_kb/sonix_keyboard_manager.o 


# Each subdirectory must supply rules for building sources it contributes
vendor/8258_dual_kb/%.o: ../vendor/8258_dual_kb/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: TC32 Compiler'
	tc32-elf-gcc -ffunction-sections -fdata-sections -I"E:\fw_workspace\trunk\source\telink\keyboard\T8_light\source" -D__PROJECT_825X_DUAL_KEYBOARD__=1 -DCHIP_TYPE=CHIP_TYPE_8258 -DUART_PRINT=0 -DSIGNATURE=0 -Wall -O2 -fpack-struct -fshort-enums -finline-small-functions -std=gnu99 -fshort-wchar -fms-extensions -c -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


