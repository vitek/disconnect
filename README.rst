Disconnect art-project firmware
===============================

Device is based on atmega128 microcontroller.

Pinout
------
 * ring: PG1 (~30Khz)
 * hang: PB5
 * debug led: PE2
 * boot_mode: PB4
 * numeric (nc): PD3
 * speaker: PORTC, 8-bit
 * AT45DB642x is connected to SPI bus
 * speaker and flash power: PE7
 * UART
 * mode: unknown

samplerate
----------

1Mhz / 4div / 12bits = 20833Hz

Authors
-------
 * Vitja Makarov
 * Viktor Popovich
