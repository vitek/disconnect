#include <avr/io.h>
#include <avr/signal.h>
#include <avr/interrupt.h>
#include <avr/delay.h>

#include <stdlib.h> /* rand, srand */

#include "config.h"

#define RING(x)                 \
    do { if (x) {               \
        PORTG &= ~(1 << PG1);   \
        PORTB &= ~(1 << PB6);   \
        PORTA &= ~(1 << PA3);   \
    } else {                    \
        PORTG |= 1 << PG1;      \
        PORTB |= 1 << PB6;      \
        PORTA |= 1 << PA3;      \
    } } while(0)

#define HANG (PIND & (1 << PD2))
#define NUM  (PIND & (1 << PD3))

/* states */
#define IDLE    0
#define ZOOM    1
#define BUSY    2
#define TALK    3
#define CALL    4
#define SILENCE 5


#define T _delay_ms(1.136/2);
#define X 100
#define SAY(x) if (NUM) PORTC = (x); else PORTC = 127;

#define BI do {                                 \
            SAY(127)            T       \
            SAY(255 - X)        T       \
            SAY(127)            T       \
            SAY(X)              T       \
        } while(0)


#define RANDOM(x) ((x) * (rand() / (RAND_MAX + 1.0)))

static inline void delay_ms(unsigned int delay)
{
    while (delay > 100) {
        _delay_ms(100);
        delay -= 100;
    }
    _delay_ms(delay);
}


static
void timer_init()
{
    ASSR = 0;
    OCR0 = OCR0_VALUE;
    TIFR &= ~((1 << OCF0) | (1 << TOV0));
    TCCR0 = (7 << CS00) | (1 << WGM01);
    TIMSK = (1 << OCIE0);
}

static volatile short timeout = -1;
static unsigned short int counter = 0;
static char state = IDLE;

static
void set_timeout(int sec) {
    timeout = INTERRUPT_FREQUENCY * sec;
}

static
void set_timeout_random(int l, int r) {
    int s;
    s = l + RANDOM(r - l);
    set_timeout(s);
}

SIGNAL(SIG_OUTPUT_COMPARE0)
{
    counter++;
    if (timeout > 0) {
        timeout--;
    }
}

static
void idle()
{
    set_timeout_random(IDLE_TIMEOUTL, IDLE_TIMEOUTR);
    while (HANG && timeout)
        asm("nop;");
    if (!timeout && HANG)
        state = CALL;
    else
        state = ZOOM;
}

static
void zoom()
{
    set_timeout(ZOOM_TIMEOUT);
    while (!HANG && NUM && timeout)
        BI;
    delay_ms(50); // jff ;)
    state = BUSY;
}

static
void busy()
{
    set_timeout(BUSY_TIMEOUT);
    while (!HANG && timeout) {
        char i = 132;
        while (i-- && !HANG)
            BI;
        i = 8;
        while (i-- && !HANG)
            _delay_ms(300);
    }
    state = SILENCE;
}

static
void silence()
{
    while (!HANG)
        asm("nop;");
    state = IDLE;
}

static unsigned char first = 1;


#include "samples.h"

static
void talk()
{
    unsigned short t = 0;
    unsigned short len;
    char i;
    PGM_P sample;

    if (first) {
        first = 0;
        srand(counter);
    }

    i = 25;

    while (i--)
        _delay_ms(100);

    /* choose sample */
    i = RANDOM(sizeof(samples) / sizeof(samples[0]));
    sample = samples[i].sample;
    len = samples[i].len;

    while (!HANG && t < len) {
        SAY(pgm_read_byte(sample + t++));

        _delay_us(62);
    }

    state = BUSY;
}

static
void call()
{
    set_timeout_random(CALL_TIMEOUTL, CALL_TIMEOUTR);

    while (HANG && timeout) {
        int j = 1500 / 30;
        while (j-- && HANG) {
            RING(j & 1);
            delay_ms(30);
        }
        RING(j & 1);

        j = 60;
        while (HANG && timeout && j--)
            delay_ms(100);
    }
    if (timeout)
        state = TALK;
    else
        state = IDLE;
}

int main()
{
    DDRC = 0xff;                // speaker
    DDRA = (1 << PA3);
    DDRB = (1 << PB6);
    DDRG = (1 << PG1);          // ring
    DDRD &= ~((1 << PD2) | (1 << PD3));         // hang

    PORTG = (1 << PG1);

    EIMSK = 0;

    cli();
    timer_init();
    sei();

    while (1) {
        switch (state) {
        case IDLE:
            idle();
            break;
        case ZOOM:
            zoom();
            break;
        case BUSY:
            busy();
            break;
        case TALK:
            talk();
            break;
        case CALL:
            call();
            break;
        case SILENCE:
            silence();
            break;
        }
    }
}
