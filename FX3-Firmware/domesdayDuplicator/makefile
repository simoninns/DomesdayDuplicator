################################################################################
# Makefile for the Domesday Duplicator FX3 firmware
#
# Note: $FX3_INSTALL_PATH should be set, example:
#       $HOME/FX3_SDK_1.3.4_Linux/fx3_firmware_linux/cyfx3sdk

#FX3PATH = ${FX3_INSTALL_PATH}

RM := rm -rf 

C_SRCS += cyfxtx.c domesdayDuplicator.c usbDescriptor.c 
S_UPPER_SRCS += cyfx_gcc_startup.S 
OBJS += cyfx_gcc_startup.o cyfxtx.o domesdayDuplicator.o usbDescriptor.o
S_UPPER_DEPS += cyfx_gcc_startup.d 
C_DEPS += cyfxtx.d domesdayDuplicator.d usbDescriptor.d 

# Each subdirectory must supply rules for building sources it contributes
%.o: %.S
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM GNU Assembler'
	arm-none-eabi-gcc -mcpu=arm926ej-s -mthumb -O3 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -Wall  -g -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

%.o: %.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=arm926ej-s -mthumb -O3 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -Wall  -g -D__CYU3P_TX__=1 -I"${FX3_INSTALL_PATH}/fw_lib/1_3_3/inc" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

# Objects
LIBS := -lcyu3lpp -lcyfxapi -lcyu3threadx -lc -lgcc

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

# All Target
all: domesdayDuplicator.elf secondary-outputs

# Tool invocations
domesdayDuplicator.elf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross ARM C Linker'
	arm-none-eabi-gcc -mcpu=arm926ej-s -mthumb -O3 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -Wall  -g -T "${FX3_INSTALL_PATH}/fw_build/fx3_fw/fx3.ld" -nostartfiles -Xlinker --gc-sections -L"${FX3_INSTALL_PATH}/fw_lib/1_3_3/fx3_release" -Wl,-Map,"domesdayDuplicator.map" -Wl,-d -Wl,--no-wchar-size-warning -Wl,--entry,CyU3PFirmwareEntry -o "domesdayDuplicator.elf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	"$(MAKE)" --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(OBJS)$(ASM_DEPS)$(S_UPPER_DEPS)$(C_DEPS) domesdayDuplicator.elf domesdayDuplicator.img domesdayDuplicator.map
	-@echo ' '

post-build:
	-@echo 'Generate boot-loadable binary image'
	-"${FX3_INSTALL_PATH}/util/elf2img/elf2img" -i domesdayDuplicator.elf -o domesdayDuplicator.img -v
	-@echo ' '

secondary-outputs:

.PHONY: all clean dependents
.SECONDARY: post-build


