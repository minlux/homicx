#ifndef CONFIG_RESPONSE_H
#define CONFIG_RESPONSE_H

/* -- Includes ------------------------------------------------------------ */
#include <stdint.h>
#include <string.h>
#include "response_helper.h"

/* -- Defines ------------------------------------------------------------- */

/* -- Types --------------------------------------------------------------- */
class ConfigResponse
{
public:
    static const uint8_t INFO_CMD_ID = 5;
    uint8_t bytes[16];

    void set_response_data(unsigned int id, uint8_t data[16])
    {
        if (id == 0x81)
        {
            memcpy(bytes, data, 16);
        }
    }

    //0.1 percent
    inline uint16_t active_power_limit()
    {
        return (((uint16_t)bytes[2] << 8) | bytes[3]);
    }

    nlohmann::json json() 
    {
        nlohmann::json result;
        result["pwrlimit"] = (double)big_endian_into_uint16(&bytes[2]) / 10;
        return result;
    }
};


/* -- Global Variables ---------------------------------------------------- */

/* -- Function Prototypes ------------------------------------------------- */

/* -- Implementation ------------------------------------------------------ */


#endif