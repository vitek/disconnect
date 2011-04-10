HOSTCC  = gcc

AS      = avr-as
CC      = avr-gcc
OBJCOPY = avr-objcopy
MCU     = atmega128
CPUFREQ = 1000000l
TIMER_HZ= 20

CFLAGS  = -g3 -mmcu=$(MCU) -Os -DF_CPU=$(CPUFREQ) -DHZ=$(TIMER_HZ) -W -Wall
ASFLAGS = $(CFLAGS)
LDFLAGS = -mmcu=$(MCU)

all: disconnect.hex

disconnect.elf: timer.o at45.o uart.o loader.o main.o crc16.o
	$(CC) $(LDFLAGS) $^ -Wl,-Map=$@.map -o $@

clean:
	rm -f *.hex *.map *.elf *.bin *.bak *~ *.o *.s *.e
	rm -f $(SAMPLES)

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $^ $@
%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $^ $@
%.s: %.c
	$(CC) $(CFLAGS) -S -g0 $^ -o $@
%.e: %.c
	$(CC) -E $(CFLAGS) -S -g0 $^ -o $@

%.8wav: %.wav
	sox $< -c 1 --rate=12500 -1 $@
