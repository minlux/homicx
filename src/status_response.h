#ifndef STATUS_RESPONSE_H
#define STATUS_RESPONSE_H

/* -- Includes ------------------------------------------------------------ */
#include <stdint.h>
#include <string.h>
#include <nlohmann/json.hpp>
#include "response_helper.h"


/* -- Defines ------------------------------------------------------------- */

/* -- Types --------------------------------------------------------------- */
class StatusResponse
{
public:
    static const uint8_t INFO_CMD_ID = 11;
    uint8_t bytes[3*16];


    void set_response_data(unsigned int id, uint8_t data[16])
    {
        if (--id <= 2) //TODO hier noch die ID 3??? (ich habe bisher nur ID 0x83 gesehen - vielleicht sollte ich diese statt der 0x03 nehmen !?)
        {
            memcpy(&bytes[16 * id], data, 16);
        }
    }

    // Function to parse the byte stream and populate JSON (0x0B RealTimeRunData_Debug)
    // concatenation of the 3 responses (0x01, 0x02, 0x03/0x83???)
    // Die auskommentieren Attribute stammen aus 0x03/0x83 ???, was im moment nicht verwendet wird!
    nlohmann::json json() 
    {
        nlohmann::json result;

        result["dc"] = nlohmann::json::array();
        result["dc"].push_back({
            { "id",      "pv1"                                             },
            { "u",       (double)big_endian_into_uint16(&bytes[ 2]) / 10   }, //V
            { "i",       (double)big_endian_into_uint16(&bytes[ 4]) / 100  }, //A
            { "p",       (double)big_endian_into_uint16(&bytes[ 6]) / 10   }, //W
            { "etotal",  (double)big_endian_into_uint32(&bytes[14]) / 1000 }, //kWh
            { "etoday",  (double)big_endian_into_uint16(&bytes[22]) / 1000 }  //kWh
        });
        result["dc"].push_back({
            { "id",      "pv2"                                             },
            { "u",       (double)big_endian_into_uint16(&bytes[ 8]) / 10   }, //V
            { "i",       (double)big_endian_into_uint16(&bytes[10]) / 100  }, //A
            { "p",       (double)big_endian_into_uint16(&bytes[12]) / 10   }, //W
            { "etotal",  (double)big_endian_into_uint32(&bytes[18]) / 1000 }, //kWh
            { "etoday",  (double)big_endian_into_uint16(&bytes[24]) / 1000 }  //kWh
        });

        result["ac"] = {
            { "u",  (double)big_endian_into_uint16(&bytes[26]) / 10  },  //V
            { "f",  (double)big_endian_into_uint16(&bytes[28]) / 100 }, //Hz
            { "p",  (double)big_endian_into_uint16(&bytes[30]) / 10  }   //W
            // { "q",  (double)big_endian_into_uint16(&bytes[32]) / 10  }, //var (Blindleistung Q, Volt-Ampere-Reactive)
            // { "i",  (double)big_endian_into_uint16(&bytes[34]) / 100 }, //A
            // { "pf", (double)big_endian_into_uint16(&bytes[36]) / 100 }  //Power-Factor
        };

        // result["temp"] = (double)big_endian_into_uint16(&bytes[38]) / 10; //Celsius (Inverter-Temperatur)
        // result["event"] = big_endian_into_uint16(&bytes[40]);
        return result;
    }
};


/* -- Global Variables ---------------------------------------------------- */

/* -- Function Prototypes ------------------------------------------------- */

/* -- Implementation ------------------------------------------------------ */

#endif