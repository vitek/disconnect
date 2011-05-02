#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <stdio.h> /* NULL */
#include <stdlib.h> /* rand */

#include "timer.h"
#include "uart.h"
#include "at45.h"
#include "crc16.h"
#include "loader.h"
#include "power.h"

#define DEBUG

#ifdef DEBUG
static int __dbg_putchar(char c, FILE *stream)
{
    static char p = 0;
    (void) stream;
    if (c == '\n' && p != '\r')
        uart0_putc('\r');
    p = c;
    uart0_putc(c);
    return 0;
}

static FILE __dbg_fp = FDEV_SETUP_STREAM(
    __dbg_putchar, NULL, _FDEV_SETUP_WRITE);

#define dbg_printf(fmt, args...) fprintf(&__dbg_fp, fmt, ## args)
#else
#define dbg_printf(fmt, ...) do {} while (0)
#endif

#define TIMER_MISC 0
#define MAX_SAMPLES 16


/* Settings */
#define BUSY_TIMES        100
#define BEEP_VOLUME       20
#define BEEP_VOLUME_READY 40

#define CALL_TIMEOUT_MIN   (3 * 60 * HZ)
#define CALL_TIMEOUT_MAX   (6 * 60 * HZ)
#define CALL_RING_MIN      10
#define CALL_RING_MAX      30

#define USER_WAIT_TIMEOUT  (2 * HZ)


typedef struct {
    uint8_t signature[4]; /* v1\r\n */
    uint8_t samples;
    uint16_t crc16;
} header_t;

typedef struct {
    unsigned char role;
    unsigned char weight;
    unsigned char repeat;
    uint16_t page;
    uint16_t pages;
    uint16_t odd;
} __attribute__((packed)) sample_t;


enum Role {
    ROLE_INCOMING = 0,
    ROLE_BUSY,
    ROLE_MUSIC,
    ROLE_NOISE,
    ROLE_MAX,
} ;

typedef struct {
    sample_t *samples[MAX_SAMPLES];
    unsigned int count;
    unsigned int weight;
} role_set_t;

static role_set_t roles[ROLE_MAX];


static sample_t all_samples[MAX_SAMPLES];
static unsigned char samples_count;


#define PANIC_FLASH_ERROR 3
#define PANIC_BAD_HEADER  2


static void panic(int n)
{
    int i, j;

    cli();
    while (1) {
        for (j = 0; j < n; j++) {
            for (i = 0; i < 1000; i++) {
                PORTG |= (1 << PG1);
                _delay_us(30);
                PORTG &= ~(1 << PG1);
                _delay_us(30);
            }
            _delay_ms(200);
        }
        _delay_ms(1000);
    }
}


static inline int
random_range(int left, int right)
{
    return rand() % (right - left) + left;
}

static int phone_read_header()
{
    header_t header;
    uint8_t *ptr;
    unsigned i;
    uint16_t crc = 0;
    unsigned int len;

    at45_read_start(0);

    ptr = (uint8_t *) &header;
    for (i = 0; i < sizeof(header_t); i++) {
        ptr[i] = at45_spi_read();
    }

    if (header.signature[0] != 'v'  || header.signature[1] != '1' ||
        header.signature[2] != '\r' || header.signature[3] != '\n')
        goto error;

    if (header.samples > MAX_SAMPLES)
        goto error;

    samples_count = header.samples;

    ptr = (uint8_t *) &all_samples;
    len = samples_count * sizeof(sample_t);

    for (i = 0; i < len; i++) {
        unsigned char c = at45_spi_read();
        crc = crc16_byte(crc, c);
        ptr[i] = c;
    }
    at45_read_stop();

    if (crc != header.crc16) {
        return -1;
    }


    for (i = 0; i < samples_count; i++) {
        sample_t *sample = &all_samples[i];
        role_set_t *role;

        if (!sample->weight)
            sample->weight = 1;
        if (sample->role >= ROLE_MAX)
            continue; /* die here? */

        role = &roles[sample->role];
        role->samples[role->count++] = sample;
        role->weight += sample->weight;
    }

    return 0;
error:
    at45_read_stop();
    return -1;
}

static inline char phone_hang()
{
    if (PINB & (1 << PB5))
        return 1;
    return 0;
}

static inline int phone_play_some(int count)
{
    while (count--) {
        PORTC = at45_spi_read();
        if (phone_hang())
            return -1;
    }

    return 0;
}

static int phone_play_sample(sample_t *sample)
{
    int retval = 0;
    unsigned i;

    cli();
    at45_read_start(sample->page);
    phone_play_some(sample->odd);

    for (i = 0; i < sample->pages; i++) {
        if (phone_play_some(AT45_PAGE_SIZE)) {
            retval = -1;
            break;
        }
    }
    at45_read_stop();
    sei();

    return retval;
}


int phone_busy()
{
    int i, j;

    for (i = 0; i < BUSY_TIMES; i++) {
        if (PINB & (1 << PB5))
            break;

        for (j = 0; j < 100; j++) {
            PORTC = 0x80 - BEEP_VOLUME;
            _delay_ms(1);
            PORTC = 0x80;
            _delay_ms(1);
            PORTC = 0x80 + BEEP_VOLUME;
            _delay_ms(1);
            PORTC = 0x80;
            _delay_ms(1);
        }

        _delay_ms(250);
    }

    return 0;
}


enum {
    RING_TIMEOUT,
    RING_ACCEPT,
} ;

int phone_ring(int count)
{
    int i;

    for (i = 0; i < count && phone_hang(); i++) {
        timer_start_oneshot(TIMER_MISC, HZ + HZ / 2);
        while (!timer_read_event(TIMER_MISC) && phone_hang()) {
            /* ~30Khz */
            PORTG |= (1 << PG1);
            _delay_us(30);
            PORTG &= ~(1 << PG1);
            _delay_us(30);
        }

        timer_start_oneshot(TIMER_MISC, HZ);
        while (!timer_read_event(TIMER_MISC) && phone_hang())
            ;
    }

    PORTG |= (1 << PG1);
    return phone_hang();
}

static
sample_t *choose_sample(enum Role role)
{
    role_set_t *set;
    unsigned int r;
    unsigned int i;

    if (role >= ROLE_MAX)
        return NULL;

    set = &roles[role];

    if (set->count == 0)
        return NULL;

    r = rand() % set->weight;

    for (i = 0; i < set->count; i++) {
        if (set->samples[i]->weight > r)
            return set->samples[i];
        r -= set->samples[i]->weight;
    }

    return NULL;
}


static int phone_call(int count)
{
    while (count--) {
        int i;

        timer_start_oneshot(TIMER_MISC, HZ / 4);
        while (!timer_read_event(TIMER_MISC)) {
            if (phone_hang())
                goto hang;
        }

        for (i = 0; i < 100; i++) {
            PORTC = 0x80 - BEEP_VOLUME_READY;
            _delay_us(500);
            PORTC = 0x80;
            _delay_us(500);
            PORTC = 0x80 + BEEP_VOLUME_READY;
            _delay_us(500);
            PORTC = 0x80;
            _delay_us(500);
        }
    }

    return 0;
hang:
    timer_stop(TIMER_MISC);
    return -1;
}


static
int phone_action_busy()
{
    sample_t *sample_music;
    sample_t *sample_message;
    int j, i;

    sample_message = choose_sample(ROLE_BUSY);
    sample_music = choose_sample(ROLE_MUSIC);

    if (!sample_message || !sample_music)
        return -1;

    timer_start_oneshot(TIMER_MISC, HZ / 2);
    while (!timer_read_event(TIMER_MISC)) {
        if (phone_hang()) {
            timer_stop(TIMER_MISC);
            return 0;
        }
    }

    if (phone_call(4))
        return 0;

    for (j = 0; j < 10; j++) {
        if (phone_play_sample(sample_message))
            return 0;

        for (i = 0; i < sample_music->repeat; i++) {
            if (phone_play_sample(sample_music))
                return 0;
        }
    }

    return phone_busy();
}

static void prnd_init()
{
    static int initialized = 0;

    if (!initialized) {
        srand(ticks);
        initialized = 1;
    }
}

/* Wait until user action or timeout */
static
int phone_wait_user(int timeout)
{
    sample_t *sample = choose_sample(ROLE_NOISE);
    unsigned long i, length;
    unsigned char old = PINB & (1 << PB6);
    int changes = 0;

    if (!sample)
        return 0;

    length = sample->odd + sample->pages * AT45_PAGE_SIZE;
    timer_start_oneshot(TIMER_MISC, timeout);

    while (1) {
        at45_read_start(sample->page);
        for (i = 0; i < length; i++) {
            if (timer_read_event(TIMER_MISC) || phone_hang() || changes > 40)
                goto done;

            PORTC = at45_spi_read();
            if (old ^ (PINB & (1 << PB6))) {
                old = PINB & (1 << PB6);
                changes++;
            }
        }
        at45_read_stop();
    }

done:
    timer_stop(TIMER_MISC);
    at45_read_stop();
    return phone_hang();
}


static
int phone_action_message()
{
    sample_t *sample;

    sample = choose_sample(ROLE_INCOMING);

    if (!sample)
        return -1;

    if (phone_ring (random_range(CALL_RING_MIN,
                                 CALL_RING_MAX)))
        return -1;
    prnd_init();

    if (phone_wait_user(USER_WAIT_TIMEOUT))
        return -1;

    if (phone_play_sample(sample))
        return -1;

    return phone_busy();
}


int main()
{
    unsigned short old_secs = 0;

    DDRC = 0xff; /* speaker */
    //DDRA = (1 << PA3);
    //DDRB = (1 << PB6);
    DDRG = (1 << PG1) ; /* ring */
    DDRE = ((1 << PE2) | /* LED */
            (1 << PE7)); /* Speaker and Flash Power */
    DDRB &= ~((1 << PB4) | /* COM on */
              (1 << PB5) | /* HANG */
              (1 << PB6)); /* Speaker */

    //DDRD &= ~((1 << PD2) | (1 << PD3));         // hang

    //PB5, PB6 - HANG

    PORTE &= ~(1 << PE7);

    if (PINB & (1 << PB4)) {
        uart_loader();
        cli();
    }

#ifdef DEBUG
    uart0_init(UART_BAUD(57600));
    dbg_printf("running phone in debug mode...\r\n");
#endif

    timer_init();
    timer_enable();

    main_power_on();
    _delay_ms(1);

    if (at45_init()) {
        panic(PANIC_FLASH_ERROR);
    }

    if (phone_read_header()) {
        panic(PANIC_BAD_HEADER);
    }

    sei();

    /* main loop */
    while (1) {
        int timeout;

        old_secs = seconds;
        timer_stop_all();
        main_power_off();

        while (!phone_hang()) {
            power_down();
        }

        timeout = random_range(CALL_TIMEOUT_MIN,
                               CALL_TIMEOUT_MAX);
        timer_start_oneshot(TIMER_MISC, timeout);

        dbg_printf("Sleeping for at least %d ticks\n", timeout);

        while (phone_hang() &&
               !timer_read_event(TIMER_MISC)) {
            power_down();
        }

        main_power_on();
        timer_stop_all();
        _delay_ms(1); /* let at45 wakeup */

        if (phone_hang()) {
            dbg_printf("Incoming call\n");
            phone_action_message();
        } else {
            if (seconds == old_secs){
                phone_busy();
            } else {
                dbg_printf("Service busy message\n");
                phone_action_busy();
            }
        }
    }
}
