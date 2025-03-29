#ifndef DEVICE_RESPONSE_H
#define DEVICE_RESPONSE_H

/* -- Includes ------------------------------------------------------------ */
#include <stdint.h>
#include <string.h>
#include <nlohmann/json.hpp>
#include "response_helper.h"


/* -- Defines ------------------------------------------------------------- */

/* -- Types --------------------------------------------------------------- */
class DeviceResponse
{
public:
    static const uint8_t INFO_CMD_ID = 1;
    uint8_t bytes[16];

    void set_response_data(unsigned int id, uint8_t data[16])
    {
        if (id == 0x81)
        {
            memcpy(bytes, data, 16);
        }
    }

    // Function to parse the byte stream and populate JSON (0x0B RealTimeRunData_Debug)
    // concatenation of the 3 responses (0x01, 0x02, 0x03)
    // Die auskommentieren Attribute stammen aus 0x03, was im moment nicht verwendet wird!
    nlohmann::json json() 
    {
        nlohmann::json result;

        const uint16_t fw_version = big_endian_into_uint16(&bytes[0]);
        const uint16_t fw_build_yyyy = big_endian_into_uint16(&bytes[2]);
        const uint16_t fw_build_mmdd = big_endian_into_uint16(&bytes[4]);
        const uint16_t fw_build_hhmm = big_endian_into_uint16(&bytes[6]);
        const uint16_t bootloader_version = big_endian_into_uint16(&bytes[8]); //oder HW Revision ???

        {
            const int major = fw_version / 10000;
            const int minor = (fw_version % 10000) / 100;
            const int patch = fw_version % 100;
            std::ostringstream version;
            version << major << "." << minor << "." << patch;
            result["firmware"] = version.str();
        }

    #if 1
        {
            const int major = bootloader_version / 10000;
            const int minor = (bootloader_version % 10000) / 100;
            const int patch = bootloader_version % 100;
            std::ostringstream version;
            version << major << "." << minor << "." << patch;
            result["bootloader"] = version.str();
        }
    #else
            result["hwrev"] = bootloader_version;
    #endif

        {
            const int month = fw_build_mmdd / 100;
            const int day = fw_build_mmdd % 100;
            const int hour = fw_build_hhmm / 100;
            const int minute = fw_build_hhmm % 100;
            std::ostringstream build_time;
            build_time
                    << fw_build_yyyy << "/"
                    << std::setw(2) << std::setfill('0') << month << "/"
                    << std::setw(2) << std::setfill('0') << day << " "
                    << std::setw(2) << std::setfill('0') << hour << ":"
                    << std::setw(2) << std::setfill('0') << minute;
            result["build"] = build_time.str();
        }

        return result;
    }
};


/* -- Global Variables ---------------------------------------------------- */

/* -- Function Prototypes ------------------------------------------------- */

/* -- Implementation ------------------------------------------------------ */


#endif