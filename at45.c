#include "at45.h"

#define OP_READ_STATUS          0xd7
#define OP_READ_CONTINUOUS      0xe8

static inline
void at45_select()
{
    PORTB &= ~(1 << PB6);
}

static inline
void at45_deselect()
{
    PORTB |= (1 << PB6);
}

static inline
unsigned char at45_status_read()
{
    unsigned char b;

    at45_select();
    at45_spi_write(OP_READ_STATUS);
    at45_spi_write(0);
    b = at45_spi_read();
    at45_deselect();
    return b;
}

char at45_init()
{
    // TODO: initialize SPI here and pins here

    switch (at45_status_read()) {
    case 0x38: /* AT45DB642x */
    case 0x3c:
        break;
    default: /* Unsupported */
        return -1;
    }

    return 0;
}
