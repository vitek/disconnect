HOSTCC  = gcc

AS      = avr-as
CC      = avr-gcc
OBJCOPY = avr-objcopy
MCU     = atmega128
CPUFREQ = 1000000l
TIMER_HZ= 100

CFLAGS  = -g3 -mmcu=$(MCU) -Os -DF_CPU=$(CPUFREQ) -DHZ=$(TIMER_HZ) -W -Wall
ASFLAGS = $(CFLAGS)
LDFLAGS = -mmcu=$(MCU)

SAMPLES_SRC = $(wildcard data/*.wav)
#samples/www.wav samples/ww1.wav
SAMPLES=$(SAMPLES_SRC:.wav=.h)

all: disconnect.hex

disconnect.elf: timer.o at45.o uart.o main.o crc16.o
	$(CC) $(LDFLAGS) $^ -Wl,-Map=$@.map -o $@

convert: convert.c
	$(HOSTCC) $(shell pkg-config --libs --cflags sndfile) $^ -o $@

samples.h: gen.sh $(SAMPLES)
	./gen.sh $(SAMPLES)

phone.o: config.h samples.h
$(SAMPLES): convert

clean:
	rm -f *.hex *.map *.elf *.bin *.bak *~ *.o *.s *.e samples.h convert
	rm -f $(SAMPLES)

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $^ $@
%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $^ $@
%.s: %.c
	$(CC) $(CFLAGS) -S -g0 $^ -o $@
%.e: %.c
	$(CC) -E $(CFLAGS) -S -g0 $^ -o $@
%.h: %.wav
	./convert sample < $^ > $@
