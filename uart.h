#ifndef DISCONNECT_UART_H
#define DISCONNECT_UART_H

#define UART_BAUD(b) (((F_CPU) / (8 * (b))) - 1)
#define UART_BUF_SIZE 64

#include <avr/io.h>

#include "irq.h"

void uart0_init(unsigned int baud);
void uart0_reset();

static inline
void uart0_putc(unsigned char c)
{
    while ( !(UCSR0A & (1<<UDRE)) )
        ;
    UDR0 = c;
}

extern volatile char _uart0_buf[UART_BUF_SIZE];
extern volatile unsigned char _uart0_buf_pos;
extern volatile unsigned char _uart0_buf_len;

static inline
unsigned char uart0_getc(unsigned char *c)
{
    unsigned char flags;

    local_irq_save(flags);

    if (0 == _uart0_buf_len) {
        local_irq_restore(flags);
        return 0;
    }

    *c = _uart0_buf[(_uart0_buf_pos - _uart0_buf_len) & (UART_BUF_SIZE - 1)];
    _uart0_buf_len--;
    local_irq_restore(flags);
    return 1;
}

static inline
unsigned char uart0_getc_noi(unsigned char *c)
{
    if ( !(UCSR0A & (1<<RXC)))
        return 0;
    *c = UDR0;
    return 1;
}

static inline
void uart0_puts(const char *msg)
{
    while (*msg)
        uart0_putc(*msg++);
}

static inline
void uart0_print_hex(unsigned char c)
{
    unsigned char c0 = c >> 4;
    unsigned char c1 = c & 0xf;

    uart0_putc(c0 > 9 ? c0 + 'a' - 10: c0 + '0');
    uart0_putc(c1 > 9 ? c1 + 'a' - 10: c1 + '0');
}

static inline
void uart0_print_hex16(unsigned short c)
{
    uart0_print_hex(c >> 8);
    uart0_print_hex(c & 0xff);
}

#endif /* DISCONNECT_UART_H */
