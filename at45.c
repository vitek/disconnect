#include <util/delay.h>

#include "at45.h"

#define OP_READ_STATUS          0xd7
#define OP_READ_CONTINUOUS      0xe8

static inline
void at45_select()
{
    PORTE &= ~(1 << PE5);
}

static inline
void at45_deselect()
{
    PORTE |= (1 << PE5);
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

int at45_init()
{
    unsigned char status;

    // MOSI and CLK are outputs
    DDRB |= (1 << PB2) | (1 << PB1) | (1 << PB0);
    DDRB &= ~(1 << PB3);
    DDRE |= (1 << PE5); /* nCS */

    SPCR = (1 << SPE) | (1 << DORD) | (1 << MSTR) | (1 << CPOL) | (1 << CPOL);

    at45_deselect();

    status = at45_status_read();

    switch (status & 0x3c) {
    case 0x38: /* AT45DB642x */
    case 0x3c:
        break;
    default: /* Unsupported */
        return -1;
    }

    return 0;
}
