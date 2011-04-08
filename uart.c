#include <avr/interrupt.h>

#include "uart.h"

void uart0_init(unsigned int baud)
{
    unsigned char cfg;

    cfg = (unsigned char) (1 << RXEN) | (1 << TXEN) | (1 << RXCIE0);

    /* Set baud rate */
    UBRR0H = (unsigned char) (baud >> 8);
    UBRR0L = (unsigned char) (baud & 0xff);

    /* Enable receiver and transmitter */
    UCSR0B = cfg;
    UCSR0A = (1 << U2X);
    /* Set frame format: 8 bits, 1 stop bit */
    UCSR0C = (unsigned char)
        (1 << UCSZ1) | (1 << UCSZ0);
}

void uart0_reset()
{
    /* disable uart */
    UCSR0B = 0;
    UCSR0A = 0;
}

volatile char _uart0_buf[UART_BUF_SIZE];
volatile unsigned char _uart0_buf_pos = 0;
volatile unsigned char _uart0_buf_len = 0;

SIGNAL(SIG_UART0_RECV)
{
    _uart0_buf[_uart0_buf_pos] = UDR0;
    _uart0_buf_pos = (_uart0_buf_pos + 1) & (UART_BUF_SIZE - 1);
    if (_uart0_buf_len < UART_BUF_SIZE)
        _uart0_buf_len++;
}
