#ifndef RESPONSE_HELPER_H
#define RESPONSE_HELPER_H

/* -- Includes ------------------------------------------------------------ */
#include <stdint.h>

/* -- Defines ------------------------------------------------------------- */

/* -- Types --------------------------------------------------------------- */

/* -- Global Variables ---------------------------------------------------- */

/* -- Function Prototypes ------------------------------------------------- */

/* -- Implementation ------------------------------------------------------ */
static inline uint16_t big_endian_into_uint16(const uint8_t bytes[])
{
    return (((uint16_t)bytes[0] << 8) | bytes[1]);
}

static inline uint32_t big_endian_into_uint32(const uint8_t bytes[])
{
    return (((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | bytes[3]);
}

#endif