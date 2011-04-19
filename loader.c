#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <string.h>

#include "timer.h"
#include "uart.h"
#include "at45.h"
#include "power.h"
#include "crc16.h"

#define TIMER_UART_TIMEOUT 0

/*
  > hi
  < disconnect vXXX
  > write XXXX
  > <PAGE DATA>
  < OK|ERR
 */

static inline const char *parse_hex(const char *args,
                                    unsigned int *dest)
{
    unsigned int result = 0;
    int len;

    while (*args == ' ' ||
           *args == '\t')
        args++;

    if (*args == '\0')
        return NULL;

    for (len = 0; ; len++) {
        unsigned char c = *args;

        if (c == '\0' || c == ' ')
            break;

        if (len >= 4)
            return NULL;

        if (c >= '0' && c <= '9')
            result = (result << 4) | (c - '0');
        else if (c >= 'a' && c <= 'f')
            result = (result << 4) | (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            result = (result << 4) | (c - 'A' + 10);
        else
            return NULL;

        args++;
    }

    if (len) {
        *dest = result;
        return args;
    }
    return NULL;
}


static int uart_loader_read_page(const char *args)
{
    unsigned int page;
    uint16_t crc = 0;
    int off;

    if (NULL == parse_hex(args, &page)) {
        uart0_puts("ERROR: read <16bit hex>\r\n");
        return -1;
    }

    uart0_puts("ok\r\n");

    if (at45_read_start(page)) {
        uart0_puts("ERROR: bad page\r\n");
        return -1;
    }

    for (off = 0; off < AT45_PAGE_SIZE; off++) {
        unsigned char c;

        c = at45_spi_read();
        crc = crc16_byte(crc, c);
        uart0_putc(c);
    }

    at45_read_stop();

    uart0_print_hex(crc >> 8);
    uart0_print_hex(crc & 0xff);
    uart0_puts("\r\n");

    return 0;
}

static int uart_loader_write_page(const char *args)
{
    unsigned int page;
    unsigned int length;
    unsigned int crc;
    uint16_t crc2 = 0;
    unsigned int i;

    if (NULL == (args = parse_hex(args, &page))) {
        goto usage;
    }

    if (NULL == (args = parse_hex(args, &length))) {
        goto usage;
    }

    if (NULL == (args = parse_hex(args, &crc))) {
        goto usage;
    }

    if (at45_write_page_start(page)) {
        uart0_puts("ERROR: at45_write_page_start() failed\r\n");
        return -1;
    }

    for (i = 0; i < length; i++) {
        unsigned char c;

        while (!uart0_getc(&c)) {
            /* TODO: handle timeout */
        }

        at45_spi_write(c);
        crc2 = crc16_byte(crc2, c);
    }

    if (at45_write_page_stop()) {
        uart0_puts("ERROR: at45_write_page_stop() failed\r\n");
        return -1;
    }

    if (crc != crc2) {
        uart0_print_hex16(crc2);
        uart0_puts("ERROR: crc16 error\r\n");
        return -1;
    }

    uart0_puts("ok\r\n");
    return 0;
usage:
    uart0_puts("ERROR: write <page> <len> <crc16>\r\n");
    return -1;
}

static void uart_loader_test()
{
    unsigned int page;
    unsigned int pos;

    at45_read_start(0);

    cli();
    for (page = 0; page < 1000; page++) {
        for (pos = 0; pos < AT45_PAGE_SIZE; pos++) {
            unsigned char c = at45_spi_read();
            PORTC = c;
        }
    }
    sei();

    SPSR &= ~(1 << SPI2X);
    SPCR = (SPCR & ~(3 << SPR0));

    at45_read_stop();


    uart0_puts("DONE\r\n");
}

#define TIMER_RING_TIMEOUT 2

static void uart_loader_ring()
{
    int i = 0;


    for (i = 0; i < 2; i++) {
        timer_start_oneshot(TIMER_RING_TIMEOUT, HZ + HZ / 2);
        while (!timer_read_event(TIMER_RING_TIMEOUT)) {
            PORTG |= (1 << PG1);
            _delay_us(30);
            PORTG &= ~(1 << PG1);
            _delay_us(30);
        }

        timer_start_oneshot(TIMER_RING_TIMEOUT, HZ);
        while (!timer_read_event(TIMER_RING_TIMEOUT)) {
            /* silence */
        }
    }

    uart0_puts("ok\r\n");
}

#define BEEP_VOLUME 20

static void uart_loader_zoom()
{
    timer_start_oneshot(TIMER_RING_TIMEOUT, HZ * 4);

    while (!timer_read_event(TIMER_RING_TIMEOUT)) {
        int i;
        for (i = 0; i < 100; i++) {
            PORTC = 0x80 - BEEP_VOLUME;
            _delay_ms(1);
            PORTC = 0x80;
            _delay_ms(1);
            PORTC = 0x80 + BEEP_VOLUME;
            _delay_ms(1);
            PORTC = 0x80;
            _delay_ms(1);
        }
    }

    uart0_puts("ok\r\n");
}

static void uart_loader_busy()
{
    int i;

    timer_start_oneshot(TIMER_RING_TIMEOUT, HZ * 4);

    while (!timer_read_event(TIMER_RING_TIMEOUT)) {
        for (i = 0; i < 100; i++) {
            PORTC = 0x80 - BEEP_VOLUME;
            _delay_ms(1);
            PORTC = 0x80;
            _delay_ms(1);
            PORTC = 0x80 + BEEP_VOLUME;
            _delay_ms(1);
            PORTC = 0x80;
            _delay_ms(1);
        }

        _delay_ms(200);
    }

    uart0_puts("ok\r\n");
}

static void uart_loader_saw()
{
    timer_start_oneshot(TIMER_RING_TIMEOUT, HZ * 4);

    while (!timer_read_event(TIMER_RING_TIMEOUT)) {
        int i;

        for (i = 0; i < 256; i++) {
            PORTC = i;
            _delay_us(1);
        }
    }

    uart0_puts("ok\r\n");
}

static int uart_loader_handle(const char *cmd)
{
    if (!strcmp(cmd, "hi")) {
        uart0_puts("disconnect v2\r\n");
    } else if (!strcmp(cmd, "go")) {
        uart0_puts("entering normal mode\r\n");
        return 1;
    } else if (!strncmp(cmd, "read ", 5)) {
        uart_loader_read_page(cmd + 5);
    } else if (!strncmp(cmd, "write ", 6)) {
        uart_loader_write_page(cmd + 6);
    } else if (!strcmp(cmd, "ring")) {
        uart_loader_ring();
    } else if (!strcmp(cmd, "zoom")) {
        uart_loader_zoom();
    } else if (!strcmp(cmd, "busy")) {
        uart_loader_busy();
    } else if (!strcmp(cmd, "saw")) {
        uart_loader_saw();
    } else if (!strcmp(cmd, "test")) {
        uart_loader_test();
    } else {
        uart0_puts("ERROR: unknown command: '");
        uart0_puts(cmd);
        uart0_puts("'\r\n");
    }

    return 0;
}

void uart_loader()
{
    unsigned int pos = 0;
    char cmd[32];

    main_power_on();

    _delay_ms(1);

    timer_init();
    uart0_init(UART_BAUD(57600));

    if (at45_init()) {
        uart0_puts("AT45 init failed\r\n");
        while (1)
            power_down();
    }

    sei();

    uart0_puts("alive\r\n");

    while (1) {
        unsigned char c;

        if (timer_read_event(TIMER_UART_TIMEOUT))
            pos = 0;

        if (uart0_getc(&c)) {
            timer_start_oneshot(TIMER_UART_TIMEOUT, HZ / 2);

            if (c == '\n') {
                cmd[pos] = 0;
                if (pos) {
                    pos = 0;
                    if (uart_loader_handle(cmd))
                        break;
                }
            } else if (c != '\r') {
                cmd[pos++] = c;
                if (pos >= sizeof(cmd))
                    pos = 0;
            }
        }
    }

    uart0_reset();
}
