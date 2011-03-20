#ifndef DISCONNECT_CRC16_H
#define DISCONNECT_CRC16_H
#include <stdint.h>

extern uint16_t const crc16_table[256];

static inline uint16_t crc16_byte(uint16_t crc, const uint8_t data)
{
    return (crc >> 8) ^ crc16_table[(crc ^ data) & 0xff];
}

#endif /* DISCONNECT_CRC16_H */

