#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "timer.h"
#include "uart.h"
#include "at45.h"
#include "crc16.h"
#include "loader.h"
#include "power.h"


typedef struct {
    uint8_t signature[4]; /* v1\r\n */
    uint8_t samples;
    uint16_t crc16;
} header_t;

typedef struct {
    unsigned char role;
    unsigned char weight;
    uint16_t page;
    uint16_t pages;
    uint16_t odd;
} sample_t;


#define MAX_SAMPLES 16

static sample_t samples[MAX_SAMPLES];
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
        header.signature[2] != '\r' || header.signature[3] != '\n') {
        return -1;
    }

    if (header.samples > MAX_SAMPLES) {
        return -1;
    }

    samples_count = header.samples;


    ptr = (uint8_t *) &samples;
    len = samples_count * sizeof(sample_t);

    for (i = 0; i < len; i++) {
        unsigned char c = at45_spi_read();
        crc = crc16_byte(crc, c);
        ptr[i] = c;
    }

    if (crc != header.crc16) {
        return -1;
    }


    at45_read_stop();
    return 0;
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

int main()
{
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
    while (1) {
        int i;

        for (i = 0; i < samples_count; i++) {
            while (PINB & (1 << PB6))
                ;

            phone_play_sample(&samples[i]);
        }
    }

    return 0;
}
