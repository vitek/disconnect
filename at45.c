#include <util/delay.h>

#include "at45.h"
#include "irq.h"
#include "uart.h"

#define OP_READ_STATUS          0xd7
#define OP_READ_CONTINUOUS      0xe8
#define OP_READ_CONTINUOUS_33   0x03

/* write to buffer, then write-erase to flash */
#define OP_PROGRAM_VIA_BUF1     0x82
#define OP_PROGRAM_VIA_BUF2     0x85

#define PAGE_OFFSET 11

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
    b = at45_spi_rdwr(0);
    at45_deselect();

    return b;
}

int at45_write_page(unsigned int page, const char *data)
{
    unsigned int addr;
    unsigned char status;
    int i;

    if (page >= AT45_NR_PAGES)
        return -1;

    status = at45_status_read();
    uart0_print_hex(status);
    uart0_puts("\r\n");


    addr = page << (PAGE_OFFSET - 8);

    at45_select();
    at45_spi_write(OP_PROGRAM_VIA_BUF1);
    at45_spi_write(addr >> 8);
    at45_spi_write(addr & 0xff);
    at45_spi_write(0);

    for (i = 0; i < AT45_PAGE_SIZE; i++)
        at45_spi_write(data[i]);

    at45_deselect();

    uart0_puts("waiting\r\n");

    do {
        status = at45_status_read();
    } while (0 == (status & 0x80));

    return 0;
}

int at45_write_page_start(unsigned int page)
{
    unsigned int addr;

    if (page >= AT45_NR_PAGES)
        return -1;

    addr = page << (PAGE_OFFSET - 8);

    at45_select();
    at45_spi_write(OP_PROGRAM_VIA_BUF1);
    at45_spi_write(addr >> 8);
    at45_spi_write(addr & 0xff);
    at45_spi_write(0);

    return 0;
}

int at45_write_page_stop()
{
    at45_deselect();

    while (0 == (at45_status_read() & 0x80))
        ;

    return 0;
}

int at45_read_start(unsigned int page)
{
    unsigned int addr;

    if (page >= AT45_NR_PAGES)
        return -1;

    addr = page << (PAGE_OFFSET - 8);

    at45_select();
    at45_spi_write(OP_READ_CONTINUOUS_33);
    at45_spi_write(addr >> 8);
    at45_spi_write(addr & 0xff);
    at45_spi_write(0);

    return 0;
}

void at45_read_stop()
{
    at45_deselect();
}

int at45_init()
{
    unsigned char status;
    unsigned char flags;


    local_irq_save(flags);
    // MOSI and CLK are outputs
    DDRB |= (1 << PB2) | (1 << PB1) | (1 << PB0);
    DDRB &= ~(1 << PB3);
    DDRE |= (1 << PE5); /* nCS */

    SPCR = (1 << SPE) | (1 << MSTR);

    at45_deselect();
    local_irq_restore(flags);

    status = at45_status_read();

    switch (status & 0x3c) {
    case 0x38: /* AT45DB642x */
    case 0x3c:
        break;
    default: /* Unsupported */
        uart0_puts("status = ");
        uart0_print_hex(status);
        uart0_puts("\r\n");
        return -1;
    }

    return 0;
}

void at45_reset()
{
    unsigned char flags;

    local_irq_save(flags);
    SPCR = 0;
    DDRB &= ~((1 << PB2) | (1 << PB1) | (1 << PB0));
    at45_deselect();
    local_irq_restore(flags);
}
