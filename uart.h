#ifndef DISCONNECT_UART_H
#define DISCONNECT_UART_H

#define UART_BAUD(b) (((F_CPU) / (8 * (b))) - 1)

#include <avr/io.h>

void uart0_init(unsigned int baud);
void uart0_reset();

static inline
void uart0_putc(unsigned char c)
{
    while ( !(UCSR0A & (1<<UDRE)) )
        ;
    UDR0 = c;
}

static inline
unsigned char uart0_getc(unsigned char *c)
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
void uart0_print_hex(char c)
{
    char c0 = c >> 4;
    char c1 = c & 0xf;

    uart0_putc(c0 > 9 ? c0 + 'a' - 10: c0 + '0');
    uart0_putc(c1 > 9 ? c1 + 'a' - 10: c1 + '0');
}

#endif /* DISCONNECT_UART_H */
