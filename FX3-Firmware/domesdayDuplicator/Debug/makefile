################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include .metadata/.plugins/org.eclipse.cdt.make.core/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: domesdayDuplicator.elf secondary-outputs

# Tool invocations
domesdayDuplicator.elf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross ARM C Linker'
	arm-none-eabi-gcc -mcpu=arm926ej-s -mthumb -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -Wall  -g3 -T "/home/sdi/Cypress/cyfx3sdk/fw_build/fx3_fw/fx3.ld" -nostartfiles -Xlinker --gc-sections -L"/home/sdi/Cypress/cyfx3sdk/fw_lib/1_3_3/fx3_debug" -Wl,-Map,"domesdayDuplicator.map" -Wl,-d -Wl,--no-wchar-size-warning -Wl,--entry,CyU3PFirmwareEntry -o "domesdayDuplicator.elf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(OBJS)$(ASM_DEPS)$(S_UPPER_DEPS)$(C_DEPS) domesdayDuplicator.elf
	-@echo ' '

post-build:
	-@echo 'Generate boot-loadable binary image'
	-/home/sdi/Cypress/cyfx3sdk/util/elf2img/elf2img -i domesdayDuplicator.elf -o domesdayDuplicator.img -v
	-@echo ' '

secondary-outputs:

.PHONY: all clean dependents
.SECONDARY: post-build

-include ../makefile.targets
