#ifndef IRQ_H
#define IRQ_H
#include <avr/io.h>
#include <avr/interrupt.h>

#define local_irq_save(x)       do {(x) = SREG; cli(); } while (0)
#define local_irq_restore(x)    do {SREG = (x); } while (0)
#endif
