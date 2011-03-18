/* AT45DB642x flash driver */
#ifndef DISCONNECT_AT45_H
#define DISCONNECT_AT45_H
#include <avr/io.h>

static inline
unsigned char at45_spi_read()
{
    while(!(SPSR & (1 << SPIF)))
        ;
    return SPDR;
}

static inline
void at45_spi_write(unsigned char b)
{
    SPDR = b;
    while (!(SPSR & (1 << SPIF)))
        ;
}

char at45_init();

#endif /* DISCONNECT_AT45_H */
