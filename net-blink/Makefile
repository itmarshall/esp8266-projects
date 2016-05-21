PROJ_NAME=net-blink
COMPORT=/dev/ttyUSB0
VPATH=.:driver
OBJS=user_main.o uart.o
CC=xtensa-lx106-elf-gcc
ESPTOOL=esptool.py
ESP8266_SDK_ROOT=~/ESP8266/esp-open-sdk/sdk
CCFLAGS= --std=c99 -Wimplicit-function-declaration -fno-inline-functions -mlongcalls -mtext-section-literals \
         -mno-serialize-volatile -I $(ESP8266_SDK_ROOT)/include -I. -I ./include -D__ETS__ -DICACHE_FLASH -DXTENSA -DUSE_US_TIMER
LDFLAGS=-nostdlib \
        -L $(ESP8266_SDK_ROOT)/lib -L $(ESP8266_SDK_ROOT)/ld -T $(ESP8266_SDK_ROOT)/ld/eagle.app.v6.ld \
        -Wl,--no-check-sections -u call_user_start -Wl,-static -Wl,--start-group \
        -lc -lgcc -lhal -lphy -lpp -lnet80211 -llwip -lwpa -lmain -ljson -lupgrade -lssl \
        -lpwm -lsmartconfig -Wl,--end-group

all: $(PROJ_NAME)_0x00000.bin

$(PROJ_NAME).out: $(OBJS)
	$(CC) -o $(PROJ_NAME).out $(LDFLAGS) $(OBJS)

$(PROJ_NAME)_0x00000.bin: $(PROJ_NAME).out
	$(ESPTOOL) elf2image --output ${PROJ_NAME}- $^

.c.o:
	$(CC) $(CCFLAGS) -c $<

clean:
	rm -f $(PROJ_NAME).out *.o *.bin

flash: all
	esptool.py --baud 230400 write_flash 0x00000 $(PROJ_NAME)-0x00000.bin 0x40000 $(PROJ_NAME)-0x40000.bin 
