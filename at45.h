/* AT45DB642x flash driver */
#ifndef DISCONNECT_AT45_H
#define DISCONNECT_AT45_H
#include <avr/io.h>

#define AT45_PAGE_SIZE 1056
#define AT45_NR_PAGES  8192

static inline
void at45_spi_write(unsigned char b)
{
    SPDR = b;
    while (!(SPSR & (1 << SPIF)))
        ;
}

static inline
unsigned char at45_spi_rdwr(unsigned char b)
{
    SPDR = b;
    while (!(SPSR & (1 << SPIF)))
        ;
    return SPDR;
}

static inline
unsigned char at45_spi_read()
{
    return at45_spi_rdwr(0);
}


int at45_init();
void at45_reset();

/**
 * Write single page.
 */
int at45_write_page(unsigned int page, const char *data);

/**
 * Issue write single page commadn
 */
int at45_write_page_start(unsigned int page);

/**
 * Wait unitil page is written
 */
int at45_write_page_stop();

/**
 * Issue continuous read command.
 * Bytes should be read manually.
 */
int at45_read_start(unsigned int page);

/**
 * Finish continuous read operation, deselect device.
 */
void at45_read_stop();


#endif /* DISCONNECT_AT45_H */
