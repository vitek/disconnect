#ifndef TIMER_H
#define TIMER_H

#ifndef HZ
# define HZ 124 /* 0.117065556712 error after 1-hour running at 8Mhz */
#endif

#define TIMER_ID_MAX 8

enum timer_mode {
    TIMER_MODE_NONE,
    TIMER_MODE_ONESHOT,
    TIMER_MODE_PERIODIC,
} ;

typedef unsigned short tick_t;
typedef unsigned char timer_id_t;

static inline
int tick_sub(tick_t a, tick_t b)
{
    return (int) (a - b);
}

void timer_init();

unsigned char timer_read_events();

int timer_start_periodic(timer_id_t id, tick_t ival);
int timer_start_oneshot(timer_id_t id, tick_t ival);
void timer_stop(timer_id_t id);
void timer_stop_all();
enum timer_mode timer_get_mode(timer_id_t id);

void timer_enable();
void timer_disable();

extern volatile tick_t ticks;
extern volatile unsigned short seconds;

#endif /* TIMER_H */
