#include <avr/io.h>
#include <avr/interrupt.h>

#include "timer.h"
#include "irq.h"

#define TIMER0_PRESCALE 1024
#define OCR0_VALUE (((F_CPU / TIMER0_PRESCALE) / HZ) - 1)

#if (OCR0_VALUE > 255) || (OCR0_VALUE < 0)
# error "Incorrect INTERRUPT_FREQUENCY"
#endif

typedef struct {
    enum timer_mode mode;
    tick_t expire;
    tick_t ival;
} timer_t;

volatile tick_t ticks;
volatile unsigned short seconds;
static volatile unsigned char timer_events = 0;
static tick_t tick_second;
static timer_t timers[TIMER_ID_MAX];


void timer_init(void)
{
    unsigned char flags;

    local_irq_save(flags);
    ticks = 0;
    ASSR = 0;
    OCR0 = OCR0_VALUE;
    TIFR = 0;
    TCCR0 = (7 << CS00) | (1 << WGM01);
    TIMSK = _BV(OCIE0);
    local_irq_restore(flags);
}

void timer_enable()
{
    TIMSK |= _BV(OCIE0);
}

void timer_disable()
{
    TIMSK &= ~_BV(OCIE0);
}

enum timer_mode timer_get_mode(timer_id_t id)
{
    return timers[id].mode;
}

int timer_start(timer_id_t id, enum timer_mode mode, tick_t ival)
{
    unsigned char flags;

    if (id >= TIMER_ID_MAX || ival < 1)
        return -1;

    local_irq_save(flags);
    timers[id].mode = mode;
    timers[id].expire = ticks + ival;
    timers[id].ival = ival;
    local_irq_restore(flags);
    return 0;
}

int timer_start_oneshot(timer_id_t id, tick_t ival)
{
    return timer_start(id, TIMER_MODE_ONESHOT, ival);
}

int timer_start_periodic(timer_id_t id, tick_t ival)
{
    return timer_start(id, TIMER_MODE_PERIODIC, ival);
}

void timer_stop(timer_id_t id)
{
    unsigned char flags;

    if (id >= TIMER_ID_MAX)
        return ;

    local_irq_save(flags);
    timers[id].mode = TIMER_MODE_NONE;
    local_irq_restore(flags);
}

void timer_stop_all()
{
    int i;

    for (i = 0; i < 8; i++)
        timer_stop(i);
}

unsigned char timer_read_events()
{
    unsigned char events;
    unsigned char flags;

    local_irq_save(flags);
    events = timer_events;
    timer_events = 0;
    local_irq_restore(flags);
    return events;
}

SIGNAL(SIG_OUTPUT_COMPARE0)
{
    int i;

    ticks++;
    tick_second++;

    if (tick_second >= HZ) {
        seconds++;
        tick_second = 0;
    }

    for (i = 0; i < TIMER_ID_MAX; i++) {
        if (timers[i].mode == TIMER_MODE_NONE)
            continue;

        if (tick_sub(ticks, timers[i].expire) >= 0) {
            timer_events |= 1 << i;

            if (timers[i].mode == TIMER_MODE_ONESHOT)
                timers[i].mode = TIMER_MODE_NONE;
            else
                timers[i].expire = ticks + timers[i].ival;
        }
    }
}
