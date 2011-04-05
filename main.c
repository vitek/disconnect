#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

#include "timer.h"
#include "uart.h"
#include "at45.h"
#include "loader.h"

static inline
void power_down()
{
    cli();

    /* switch off leds */
    PORTE |= (1 << PE2);
    PORTG |= (1 << PG1);

    /* switch off speaker power */
    PORTE |= (1 << PE7);

    set_sleep_mode(SLEEP_MODE_IDLE);
    sei();
    sleep_mode();
}

int main()
{
    DDRC = 0xff; /* speaker */
    //DDRA = (1 << PA3);
    //DDRB = (1 << PB6);
    DDRG = (1 << PG1) ; /* ring */
    DDRE = ((1 << PE2) | /* LED */
            (1 << PE7)); /* Speaker Power */
    DDRB &= ~((1 << PB4) | /* COM on */
              (1 << PB5)); /* HANG */

    //DDRD &= ~((1 << PD2) | (1 << PD3));         // hang

    //PB5, PB6 - HANG

    PORTE &= ~(1 << PE7);

    timer_init();
    timer_enable();


    unsigned short oldsecs = 0;

    sei();

    uart_loader();

    while (1) {
        power_down();

        if (oldsecs != seconds) {
            uart0_puts("ticksr\r\n");
            oldsecs = seconds;
        }


/*        PORTE |= (1 << PE2);
        PORTG |= (1 << PG1);
        PORTE |= (1 << PE7);
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_mode(); */
    }

    return 0;
}
