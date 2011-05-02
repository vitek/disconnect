#ifndef DISCONNECT_POWER_H
#define DISCONNECT_POWER_H
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "irq.h"

static inline
void power_down()
{
    unsigned char flags;

    local_irq_save(flags);
    /* switch off leds */
    PORTE |= (1 << PE2);
    PORTG |= (1 << PG1);

    /* switch off speaker power */
    PORTE |= (1 << PE7);

    set_sleep_mode(SLEEP_MODE_IDLE);
    local_irq_restore(flags);

    sleep_mode();
}

static inline
void main_power_on()
{
    PORTE &= ~(1 << PE7);
}

static inline
void main_power_off()
{
    PORTE |= (1 << PE7);
}

#endif /* DISCONNECT_POWER_H */
