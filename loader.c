#include <avr/io.h>
#include <avr/interrupt.h>

#include <string.h>

#include "timer.h"
#include "uart.h"

#define TIMER_UART_TIMEOUT 0

/*
  > hi
  < disconnect vXXX
  > write XXXX
  > <PAGE DATA>
  < OK|ERR
 */

static int uart_loader_handle(const char *cmd)
{
    if (!strcmp(cmd, "hi")) {
        uart0_puts("disconnect v2\r\n");
    } else if (!strcmp(cmd, "go")) {
        uart0_puts("entering normal mode\r\n");
        return 1;
    }

    return 0;
}

void uart_loader()
{
    unsigned int pos = 0;
    char cmd[16];

    timer_init();
    uart0_init(UART_BAUD(115200));

    sei();

    while (1) {
        unsigned char c;

        if (timer_read_event(TIMER_UART_TIMEOUT))
            pos = 0;

        if (uart0_getc(&c)) {
            timer_start_oneshot(TIMER_UART_TIMEOUT, HZ / 2);

            if (c == '\n' || c == '\r') {
                cmd[pos] = 0;
                if (pos) {
                    pos = 0;
                    if (uart_loader_handle(cmd))
                        break;
                }
            } else {
                cmd[pos++] = c;
                if (pos >= sizeof(cmd))
                    pos = 0;
            }
        }
    }

    uart0_reset();
}
