#ifndef DISCONNECT_UART_H
#define DISCONNECT_UART_H

#define UART_BAUD(b) (((F_CPU) / (8 * (b))) - 1)

#include <avr/io.h>

void uart0_init(unsigned int baud);

static inline
void uart0_putc(unsigned char c)
{
    while ( !(UCSR0A & (1<<UDRE)) )
        ;
    UDR0 = c;
}

unsigned char uart0_getc(unsigned char *c)
{
    if ( !(UCSR0A & (1<<RXC)))
        return 0;
    *c = UDR0;
    return 1;
}
#endif /* DISCONNECT_UART_H */
