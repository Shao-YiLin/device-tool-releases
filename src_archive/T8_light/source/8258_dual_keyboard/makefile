################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include vendor/common/subdir.mk
-include vendor/8258_dual_kb/subdir.mk
-include third/voice/subdir.mk
-include third/protocol/subdir.mk
-include third/md5/subdir.mk
-include third/hash/subdir.mk
-include third/fifo/subdir.mk
-include third/crc/subdir.mk
-include drivers/8258/subdir.mk
-include common/subdir.mk
-include boot/8258/subdir.mk
-include boot/8253/subdir.mk
-include boot/8251/subdir.mk
-include application/usbstd/subdir.mk
-include application/print/subdir.mk
-include application/keyboard/subdir.mk
-include application/app/subdir.mk
-include objects.mk

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
LST += \
8258_dual_keyboard.lst \

FLASH_IMAGE += \

SIZEDUMMY += \
sizedummy \


# All Target
all: T8_light.elf secondary-outputs

# Tool invocations
T8_light.elf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: TC32 C Linker'
	tc32-elf-ld --gc-sections -L"E:\fw_workspace\trunk\source\telink\keyboard\T8_light\source\proj_lib" -T ../boot.link -o"T8_light.elf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

8258_dual_keyboard.lst: T8_light.elf
	@echo 'Invoking: TC32 Create Extended Listing'
	tc32-elf-objdump -x -D -l -S T8_light.elf  >"8258_dual_keyboard.lst"
	@echo 'Finished building: $@'
	@echo ' '

: T8_light.elf
	@echo 'Create Flash image (binary format)'
	tc32-elf-objcopy -O binary T8_light.elf
	@echo 'Finished building: $@'
	@echo ' '

sizedummy: T8_light.elf
	@echo 'Invoking: Print Size'
	tc32-elf-size -t T8_light.elf
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(FLASH_IMAGE)$(ELFS)$(OBJS)$(LST)$(SIZEDUMMY) T8_light.elf
	-@echo ' '

post-build:
	-"E:\fw_workspace\trunk\source\telink\keyboard\T8_light\source/tl_check_fw.sh"   8258_dual_keyboard   T8_light
	-@echo ' '

secondary-outputs: $(LST) $(FLASH_IMAGE) $(SIZEDUMMY)

.PHONY: all clean dependents
.SECONDARY: post-build

-include ../makefile.targets
