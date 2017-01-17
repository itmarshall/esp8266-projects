#
# Makefile for the Delta inverter gateway.
#
# Based on the makefile from the JeeLabs esp-link - https://github.com/jeelabs/esp-link
# Original from esphttpd and others...
# VERBOSE=1
#
# Start by setting the directories for the toolchain a few lines down
# the default target will build the firmware images
# `make flash` will flash the esp serially
# `make tcpflash` will flash the esp over wifi
# `VERBOSE=1 make ...` will print debug info
# `ESP_HOSTNAME=my.esp.example.com make wiflash` is an easy way to override a variable

# The name of the project being built.
PROJ_NAME ?= delta_reader

# hostname or IP address for OTA flashing
#ESP_HOSTNAME ?= esp8266
ESP_HOSTNAME ?= 10.0.1.22

# --------------- toolchain configuration ---------------

# Base directory for the compiler. Needs a / at the end.
# Typically you'll install https://github.com/pfalcon/esp-open-sdk
XTENSA_TOOLS_ROOT ?= ~/ESP8266/esp-open-sdk/xtensa-lx106-elf/bin/

# Firmware version 
#SDK_VERS ?= esp_iot_sdk_v1.5.2
SDK_VERS ?= ESP8266_NONOS_SDK_V1.5.4_16_05_20

# Try to find the firmware manually extracted, e.g. after downloading from Espressif's BBS,
# http://bbs.espressif.com/viewforum.php?f=46
SDK_BASE ?= $(wildcard ../$(SDK_VERS))

# If the firmware isn't there, see whether it got downloaded as part of esp-open-sdk
ifeq ($(SDK_BASE),)
SDK_BASE := $(wildcard $(XTENSA_TOOLS_ROOT)/../../$(SDK_VERS))
endif

# Clean up SDK path
SDK_BASE := $(abspath $(SDK_BASE))
$(warning Using SDK from $(SDK_BASE))

# Path to bootloader file
BOOTFILE	?= $(SDK_BASE/bin/boot_v1.5.bin)

# Esptool.py path and port, only used for 1-time serial flashing
# Typically you'll use https://github.com/themadinventor/esptool
# Windows users use the com port i.e: ESPPORT ?= com3
ESPTOOL		?= ~/ESP8266/esp-open-sdk/esptool/esptool.py
ESPPORT		?= /dev/ttyUSB0
ESPBAUD		?= 460800
#ESPBAUD		?= 57600

# --------------- chipset configuration   ---------------

# Pick your flash size: "512KB", "1MB", or "4MB"
FLASH_SIZE ?= 4MB

# -------------- End of config options -------------

HTML_PATH = $(abspath ./html)/
WIFI_PATH = $(HTML_PATH)wifi/

ESP_FLASH_MAX       ?= 503808  # max bin file

ifeq ("$(FLASH_SIZE)","512KB")
# Winbond 25Q40 512KB flash, typ for esp-01 thru esp-11
ESP_SPI_SIZE        ?= 0       # 0->512KB (256KB+256KB)
ESP_FLASH_MODE      ?= 0       # 0->QIO
ESP_FLASH_FREQ_DIV  ?= 0       # 0->40Mhz
ET_FS               ?= 4m      # 4Mbit flash size in esptool flash command
ET_FF               ?= 40m     # 40Mhz flash speed in esptool flash command
ET_BLANK            ?= 0x7E000 # where to flash blank.bin to erase wireless settings

else ifeq ("$(FLASH_SIZE)","1MB")
# ESP-01E
ESP_SPI_SIZE        ?= 2       # 2->1MB (512KB+512KB)
ESP_FLASH_MODE      ?= 0       # 0->QIO
ESP_FLASH_FREQ_DIV  ?= 15      # 15->80MHz
ET_FS               ?= 8m      # 8Mbit flash size in esptool flash command
ET_FF               ?= 80m     # 80Mhz flash speed in esptool flash command
ET_BLANK            ?= 0xFE000 # where to flash blank.bin to erase wireless settings

else ifeq ("$(FLASH_SIZE)","2MB")
# Manuf 0xA1 Chip 0x4015 found on wroom-02 modules
# Here we're using two partitions of approx 0.5MB because that's what's easily available in terms
# of linker scripts in the SDK. Ideally we'd use two partitions of approx 1MB, the remaining 2MB
# cannot be used for code (esp8266 limitation).
ESP_SPI_SIZE        ?= 4       # 6->4MB (1MB+1MB) or 4->4MB (512KB+512KB)
ESP_FLASH_MODE      ?= 0       # 0->QIO, 2->DIO
ESP_FLASH_FREQ_DIV  ?= 15      # 15->80Mhz
ET_FS               ?= 16m     # 16Mbit flash size in esptool flash command
ET_FF               ?= 80m     # 80Mhz flash speed in esptool flash command
ET_BLANK            ?= 0x1FE000 # where to flash blank.bin to erase wireless settings

else
# Winbond 25Q32 4MB flash, typ for esp-12
# Here we're using two partitions of approx 0.5MB because that's what's easily available in terms
# of linker scripts in the SDK. Ideally we'd use two partitions of approx 1MB, the remaining 2MB
# cannot be used for code (esp8266 limitation).
ESP_SPI_SIZE        ?= 4       # 6->4MB (1MB+1MB) or 4->4MB (512KB+512KB)
ESP_FLASH_MODE      ?= 0       # 0->QIO, 2->DIO
ESP_FLASH_FREQ_DIV  ?= 15      # 15->80Mhz
ET_FS               ?= 32m     # 32Mbit flash size in esptool flash command
ET_FF               ?= 80m     # 80Mhz flash speed in esptool flash command
ET_BLANK            ?= 0x3FE000 # where to flash blank.bin to erase wireless settings
endif

# --------------- version ---------------

# This queries git to produce a version string like "ota-tcp v0.9.0 2015-06-01 34bc76"
#VERSION ?= "$(PROJ_NAME) custom version"
DATE    := $(shell date '+%F %T')
#BRANCH  ?= $(shell if git diff --quiet HEAD; then git describe --tags; \
#                   else git symbolic-ref --short HEAD; fi)
#SHA     := $(shell if git diff --quiet HEAD; then git rev-parse --short HEAD | cut -d"/" -f 3; \
#                   else echo "development"; fi)
#VERSION ?=$(PROJ_NAME) $(BRANCH) - $(DATE) - $(SHA)
VERSION ?=$(PROJ_NAME) - $(DATE)

# Output directors to store intermediate compiled files
# relative to the project directory
BUILD_BASE	= build
FW_BASE		= firmware

# name for the target project
#TARGET		= httpd
TARGET		= $(PROJ_NAME)

# espressif tool to concatenate sections for OTA upload using bootloader v1.2+
APPGEN_TOOL	?= gen_appbin.py

CFLAGS=

# set defines for optional modules
ifneq (,$(findstring mqtt,$(MODULES)))
	CFLAGS		+= -DMQTT
endif

ifneq (,$(findstring rest,$(MODULES)))
	CFLAGS		+= -DREST
endif

ifneq (,$(findstring syslog,$(MODULES)))
	CFLAGS		+= -DSYSLOG
endif

# which modules (subdirectories) of the project to include in compiling
LIBRARIES_DIR 	= libraries
#MODULES         += src
MODULES         += driver src
MODULES			+= $(foreach sdir,$(LIBRARIES_DIR),$(wildcard $(sdir)/*))
EXTRA_INCDIR 	= include .

# libraries used in this project, mainly provided by the SDK
LIBS = c gcc hal phy pp net80211 wpa main lwip json upgrade ssl

# compiler flags using during compilation of source files
CFLAGS	+= -Os -ggdb -std=c99 -Werror -Wpointer-arith -Wl,-EL -fno-inline-functions \
		-nostdlib -mlongcalls -mtext-section-literals -ffunction-sections -fdata-sections \
		-D__ets__ -DICACHE_FLASH -Wno-address -DFIRMWARE_SIZE=$(ESP_FLASH_MAX) \
		-DVERSION="$(VERSION)"

# linker flags used to generate the main object file
LDFLAGS		= -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static -Wl,--gc-sections

# various paths from the SDK used in this project
SDK_LIBDIR		= lib
SDK_LDDIR		= ld
SDK_INCDIR		= include
SDK_TOOLSDIR	= tools

# select which tools to use as compiler, librarian and linker
CC		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
AR		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-ar
LD		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
OBJCP	:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objcopy
OBJDP	:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objdump


####
SRC_DIR		:= $(MODULES)
BUILD_DIR	:= $(addprefix $(BUILD_BASE)/,$(MODULES))

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_LDDIR 	:= $(addprefix $(SDK_BASE)/,$(SDK_LDDIR))
SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))
SDK_TOOLS	:= $(addprefix $(SDK_BASE)/,$(SDK_TOOLSDIR))
APPGEN_TOOL	:= $(addprefix $(SDK_TOOLS)/,$(APPGEN_TOOL))

SRC			:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ			:= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC))
LIBS		:= $(addprefix -l,$(LIBS))
APP_AR		:= $(addprefix $(BUILD_BASE)/,$(TARGET)_app.a)
USER1_OUT 	:= $(addprefix $(BUILD_BASE)/,$(TARGET).user1.out)
USER2_OUT 	:= $(addprefix $(BUILD_BASE)/,$(TARGET).user2.out)

INCDIR			:= $(addprefix -I,$(SRC_DIR))
EXTRA_INCDIR	:= $(addprefix -I,$(EXTRA_INCDIR))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))

# linker script used for the above linker step
LD_SCRIPT1	:= $(SDK_LDDIR)/eagle.app.v6.new.1024.app1.ld
LD_SCRIPT2	:= $(SDK_LDDIR)/eagle.app.v6.new.1024.app2.ld

V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q)$(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CFLAGS)  -c $$< -o $$@
endef

.PHONY: all checkdirs clean tcpflash

all: echo_version checkdirs $(FW_BASE)/user1.bin $(FW_BASE)/user2.bin

echo_version:
	@echo VERSION: $(VERSION)

$(USER1_OUT): $(APP_AR)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) -T$(LD_SCRIPT1) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@
	@echo Dump  : $(OBJDP) -x $(USER1_OUT)
	@echo Disass: $(OBJDP) -d -l -x $(USER1_OUT)

$(USER2_OUT): $(APP_AR)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) -T$(LD_SCRIPT2) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@

$(FW_BASE):
	$(vecho) "FW $@"
	$(Q) mkdir -p $@

$(FW_BASE)/user1.bin: $(USER1_OUT) $(FW_BASE)
	$(Q) $(OBJCP) --only-section .text -O binary $(USER1_OUT) eagle.app.v6.text.bin
	$(Q) $(OBJCP) --only-section .data -O binary $(USER1_OUT) eagle.app.v6.data.bin
	$(Q) $(OBJCP) --only-section .rodata -O binary $(USER1_OUT) eagle.app.v6.rodata.bin
	$(Q) $(OBJCP) --only-section .irom0.text -O binary $(USER1_OUT) eagle.app.v6.irom0text.bin
	ls -ls eagle*bin
	$(Q) COMPILE=gcc PATH=$(XTENSA_TOOLS_ROOT):$(PATH) python $(APPGEN_TOOL) $(USER1_OUT) 2 $(ESP_FLASH_MODE) $(ESP_FLASH_FREQ_DIV) $(ESP_SPI_SIZE) 0
	$(Q) rm -f eagle.app.v6.*.bin
	$(Q) mv eagle.app.flash.bin $@
	@echo "** user1.bin uses $$(stat -c '%s' $@) bytes of" $(ESP_FLASH_MAX) "available"
	$(Q) if [ $$(stat -c '%s' $@) -gt $$(( $(ESP_FLASH_MAX) )) ]; then echo "$@ too big!"; false; fi

$(FW_BASE)/user2.bin: $(USER2_OUT) $(FW_BASE)
	$(Q) $(OBJCP) --only-section .text -O binary $(USER2_OUT) eagle.app.v6.text.bin
	$(Q) $(OBJCP) --only-section .data -O binary $(USER2_OUT) eagle.app.v6.data.bin
	$(Q) $(OBJCP) --only-section .rodata -O binary $(USER2_OUT) eagle.app.v6.rodata.bin
	$(Q) $(OBJCP) --only-section .irom0.text -O binary $(USER2_OUT) eagle.app.v6.irom0text.bin
	$(Q) COMPILE=gcc PATH=$(XTENSA_TOOLS_ROOT):$(PATH) python $(APPGEN_TOOL) $(USER2_OUT) 2 $(ESP_FLASH_MODE) $(ESP_FLASH_FREQ_DIV) $(ESP_SPI_SIZE) 0
	$(Q) rm -f eagle.app.v6.*.bin
	$(Q) mv eagle.app.flash.bin $@
	$(Q) if [ $$(stat -c '%s' $@) -gt $$(( $(ESP_FLASH_MAX) )) ]; then echo "$@ too big!"; false; fi

$(APP_AR): $(OBJ)
	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $^

checkdirs: $(BUILD_DIR)

$(BUILD_DIR):
	$(Q) mkdir -p $@

tcpflash: all
	./tcp_flash.py $(ESP_HOSTNAME) $(FW_BASE)/user1.bin $(FW_BASE)/user2.bin

baseflash: all
	$(Q) $(ESPTOOL) --port $(ESPPORT) --baud $(ESPBAUD) write_flash 0x01000 $(FW_BASE)/user1.bin

flash: all
	$(Q) $(ESPTOOL) --port $(ESPPORT) --baud $(ESPBAUD) write_flash -fs $(ET_FS) -ff $(ET_FF) \
	  0x00000 "$(SDK_BASE)/bin/boot_v1.5.bin" 0x01000 $(FW_BASE)/user1.bin \
	  $(ET_BLANK) $(SDK_BASE)/bin/blank.bin

clean:
	$(Q) rm -f $(APP_AR)
	$(Q) rm -f $(TARGET_OUT)
	$(Q) find $(BUILD_BASE) -type f | xargs rm -f
	$(Q) rm -rf $(FW_BASE)

$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))
