#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <iostream>
#include <thread>
#include <string>
#include <atomic>
#include <unistd.h>    // usleep()
#include <RF24/RF24.h> // See http://nRF24.github.io/RF24/pages.html for more information on usage
#include "argtable3.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "event_dispatcher.h"
#include "sequence.h"
#include "crc.h"
#include "response_helper.h"
#include "status_response.h"
#include "config_response.h"
#include "device_response.h"


/* -- Defines ------------------------------------------------------------- */
#define APP_NAME "homicx"
#define APP_NAME_PRETTY "Hoymiles Microinverter Control"
#define APP_VERSION "0.1.0"

#define DTU_SERIAL "999970535453"


/* -- Types --------------------------------------------------------------- */
// using namespace std;


/* -- (Module-)Global Variables ------------------------------------------- */
static bool verbose;

static RF24 radio(22, 0, 2500000); //CEn, CS, 2.5MBit/s
static Sequence txChannel(std::vector<uint8_t>{3, 23, 40, 61, 75});
static Sequence rxChannel(std::vector<uint8_t>{3, 23, 40, 61, 75});
static Sequence info(std::vector<uint8_t>{1, 5, 11, 11, 11, 11}); //hardware-info, parameter, realtime data
static DeviceResponse device;
static ConfigResponse config;
static StatusResponse status;
static int powerLimit = -1;

static nlohmann::json inverterStatus;
static EventDispatcher eventDispatcher;

/* -- Function Prototypes ------------------------------------------------- */
static bool request_info(const uint8_t dtuHmAddr[4], const uint8_t wrHmAddr[4], uint8_t cmd);
static bool set_active_power_limit(const uint8_t dtuHmAddr[4], const uint8_t wrHmAddr[4], uint16_t limit);
static bool restart_inverter(const uint8_t dtuHmAddr[4], const uint8_t wrHmAddr[4]);
static bool transmit(const uint8_t * data, size_t len);
static void init_receiver();
static void next_rx_channel();
static unsigned int receive(uint8_t buffer[32]);


/* -- Implementation ------------------------------------------------------ */

static uint32_t get_time_1ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(1000u * ts.tv_sec + (ts.tv_nsec / 1000000u));
}


//For verbose logging only
//Get timestamp in format like this: 2022-05-02 16:41:16.044
static char * get_timestamp(char buffer[24])
{
    struct timeval tv;
    gettimeofday(&tv, NULL);  // Get current time with microseconds
    struct tm *local_tm = localtime(&tv.tv_sec);  // Convert to local time

    // Print timestamp with get_time_1mseconds
    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
           local_tm->tm_year + 1900, local_tm->tm_mon + 1, local_tm->tm_mday,
           local_tm->tm_hour, local_tm->tm_min, local_tm->tm_sec,
           tv.tv_usec / 1000);
    return buffer;
}


//For verbose logging only
//Print hexdump of given data (with optional seperator)
static char * get_hexdump(char * destination, const uint8_t * source, unsigned int num, char separator)
{
    const char HEX_DIGITS[16] =
    {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };
    unsigned int count = 0;
    if (num > 0) //at least one source byte
    {
        while (1)
        {
            unsigned int d = *source++;
            destination[count++] = HEX_DIGITS[d >> 4];
            destination[count++] = HEX_DIGITS[d & 0xF];
            if (--num > 0) //at least one *more* source byte
            {
                if (separator != 0) //need to add an separator char?
                {
                    destination[count++] = separator;
                }
                continue;
            }
            break; //otherwise, we are done
        }

    }
    destination[count] = 0; //zero termination
    return destination;
}


// Extract IP address and port from *ip_port* given in the format "[ip:]port", where ip is optional.
// If the ip is not provided, the value of *ip* shall be unchanged.
// return 1, if the both - ip and port - was extracted
// return 0, if only port was extracted
static int extract_ip_port(const char *ip_port, std::string &ip, uint16_t &port) {
    // Find the colon character, if it exists
    const char *colon = strchr(ip_port, ':');
    if (colon) { // IP is specified
        ip = std::string(ip_port, colon - ip_port);  //copy substring before ':' into ip
        port = (uint16_t)atoi(colon + 1);
        return 1;
    }

    // No IP specified, just get port
    port = (uint16_t)atoi(ip_port);
    return 0;
}


//SN 114172220143 -> HM 72 22 01 43
static int serno_to_hmaddr(const char * sn, uint8_t hmAddr[4])
{
    const int len = strlen(sn);
    if (len >= 8)
    {
        hmAddr[0]  = (sn[len - 8] & 0x0F) << 4;
        hmAddr[0] |= (sn[len - 7] & 0x0F);
        hmAddr[1]  = (sn[len - 6] & 0x0F) << 4;
        hmAddr[1] |= (sn[len - 5] & 0x0F);
        hmAddr[2]  = (sn[len - 4] & 0x0F) << 4;
        hmAddr[2] |= (sn[len - 3] & 0x0F);
        hmAddr[3]  = (sn[len - 2] & 0x0F) << 4;
        hmAddr[3] |= (sn[len - 1] & 0x0F);
        return 0;
    }
    return -1;
}


int main(int argc, char *argv[])
{
    struct arg_lit *argHelp;
    struct arg_lit *argVersion;
    struct arg_str *argSerial;
    struct arg_int *argPoll;
    struct arg_str *argListen;
    struct arg_str *argServe;
    struct arg_lit *argVerbose;
    struct arg_end *argEnd;
    void *argtable[] =
    {
        argHelp = arg_lit0(NULL, "help", "Print help and exit"),
        argVersion = arg_lit0(NULL, "version", "Print version and exit"),
        argSerial = arg_str1(NULL, "serial", "<SERIAL>", "Serial number of microinverter"),
        argPoll = arg_int0(NULL, "poll", "<INTERVAL>", "Poll inverter every INTERVAL seconds (default=5)"),
        argListen = arg_str1(NULL, "listen", "<[IP:]PORT>", "HTTP server listen address (default IP=0.0.0.0)"),
        argServe = arg_str0(NULL, "serve", "<PATH>", "Path to directory statically served by HTTP server"),
        argVerbose = arg_lit0(NULL, "verbose", "Be verbose"),
        argEnd = arg_end(5),
    };

    // Parse command line arguments.
    int err = arg_parse(argc, argv, argtable);
    verbose = (argVerbose->count != 0);

    // Help
    if (argHelp->count)
    {
        // Description
        puts(APP_NAME_PRETTY " V" APP_VERSION);
        // Usage
        arg_print_syntax(stdout, argtable, "\n\n");
        // Options
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        return 0;
    }

    // Version
    if (argVersion->count)
    {
        puts(APP_VERSION);
        return 0;
    }

    // If the parser returned any errors then display them and exit
    if (err > 0)
    {
        // Display the error details contained in the arg_end struct.
        arg_print_errors(stdout, argEnd, APP_NAME);
        fprintf(stderr, "%s: Try '--help' for more information\n", APP_NAME);
        return -1;
    }

    // Get serial number
    const char * const wrSerial = argSerial->sval[0];
    uint8_t wrHmAddr[4];
    uint8_t dtuHmAddr[4];
    if (serno_to_hmaddr(wrSerial, wrHmAddr) != 0)
    {
        fprintf(stderr, "%s: Invalid serial number (at least 8 decimal digits expected)\n", APP_NAME);
        return -1;
    }
    serno_to_hmaddr(DTU_SERIAL, dtuHmAddr);

    // Get listen ip, port
    std::string listenIp = "0.0.0.0"; //preset default
    uint16_t listenPort = 0;
    extract_ip_port(argListen->sval[0], listenIp, listenPort); //get actual listen port and - if specified - the actual listen ip.
    if (listenPort == 0)
    {
        fprintf(stderr, "%s: Invalid listen address\n", APP_NAME);
        return -2;
    }

    // Initialize nrf24 radio
    if (!radio.begin())
    {
        fprintf(stderr, "%s: Radio hardware is not responding\n", APP_NAME);
        return -3;
    }
    if(!radio.isPVariant())
    {
        fprintf(stderr, "%s: Radio is not a nRF24L01+\n", APP_NAME);
        return -4;
    }
#if 1 //sollte doch schon durch radio.begin() geprueft sein !?
    if(!radio.isChipConnected())
    {
        fprintf(stderr, "%s: Chip is not connected to SPI\n", APP_NAME);
        return -5;
    }
#endif

    // Start HTTP Server
    httplib::Server svr;

    if (argServe->count)
    {
        svr.set_mount_point("/", argServe->sval[0]);
    }

    // Wildcard OPTIONS handler for CORS preflight requests
    svr.Options("/.*", [&](const httplib::Request &req, httplib::Response & res)
    {
        res.set_header("Access-Control-Allow-Headers", "*");
        res.set_header("Access-Control-Allow-Methods", "*");
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    svr.Get("/app", [&](const httplib::Request & /*req*/, httplib::Response &res)
    {
        // Create a JSON object
        nlohmann::json j;
        j["name"] = APP_NAME;
        j["brief"] = APP_NAME_PRETTY;
        j["version"] = APP_VERSION;

        // Set response content type and send JSON
        res.set_header("Access-Control-Allow-Headers", "*");
        res.set_header("Access-Control-Allow-Methods", "*");
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(j.dump(), "application/json");
    });

    // Handle GET request and return JSON response
    svr.Get("/status", [](const httplib::Request&, httplib::Response& res)
    {
        // Set response content type and send JSON
        res.set_header("Access-Control-Allow-Headers", "*");
        res.set_header("Access-Control-Allow-Methods", "*");
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(inverterStatus.dump(), "application/json");
    });

    // GET /power?limit=x
    svr.Get("/power", [](const httplib::Request &req, httplib::Response &res)
    {
        const int limit = std::strtol(req.get_param_value("limit").c_str(), nullptr, 10); //get value of query parameter limit
        powerLimit = limit;
        res.set_header("Access-Control-Allow-Headers", "*");
        res.set_header("Access-Control-Allow-Methods", "*");
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    // Server-Sent-Events
    svr.Get("/subscribe", [&](const httplib::Request & /*req*/, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Headers", "*");
        res.set_header("Access-Control-Allow-Methods", "*");
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Connection", "keep-alive");
        res.set_content_provider(
            "text/event-stream",
            [&](size_t /*offset*/, httplib::DataSink &sink) {
                eventDispatcher.wait_event(&sink);
                return true;
            }
        );
    });

#if 0 //for testing only
    // curl -X POST http://localhost:8088/publish -H "Content-Type: application/json" -d '{"name": Manuel, "cnt": 7}'
    svr.Post("/publish", [&](const httplib::Request &req, httplib::Response & res)
    {
        std::string body = req.body;
        eventDispatcher.send_event(std::move(body));
        // nlohmann::json body = nlohmann::json::parse(req.body);

        res.set_header("Access-Control-Allow-Headers", "*");
        res.set_header("Access-Control-Allow-Methods", "*");
        res.set_header("Access-Control-Allow-Origin", "*");
    });
#endif


    // Run the server in a separate thread
    std::thread server_thread([&]() {
        if (verbose) printf("Listen to %s:%u\n", listenIp.c_str(), listenPort);
        svr.listen(listenIp, listenPort);
    });


    // Greetings
    if (verbose)
    {
        printf("WR SN %s\n", wrSerial);
        printf("WR HM Addr:  %02X:%02X:%02X:%02X\n", wrHmAddr[0], wrHmAddr[1], wrHmAddr[2], wrHmAddr[3]);
        printf("DTU SN %s\n", DTU_SERIAL);
        printf("DTU HM Addr:  %02X:%02X:%02X:%02X\n", dtuHmAddr[0], dtuHmAddr[1], dtuHmAddr[2], dtuHmAddr[3]);
    }


    const uint32_t poll1ms = (argPoll->count) ? 1000u * (uint32_t)argPoll->ival[0] : 5000u;
    uint32_t ttx = 0;
    uint32_t trx = 0;
    uint8_t buffer[32];
    unsigned int rxlen = 0;
    while (true)
    {
        const uint32_t time1ms = get_time_1ms();
        if ((time1ms - ttx) >= poll1ms)
        {
            //if we got a response to systemConfigParam request 
            // and we see that the inverter has a different active power limit than requested
            if ((powerLimit >= 0) && ((uint16_t)powerLimit != config.active_power_limit()))
            {
                set_active_power_limit(dtuHmAddr, wrHmAddr, (uint16_t)powerLimit); //set new power limit
                info.reset(); //reset + the following increment brings us to info command 5 which is what i want to read back the active power limit
                request_info(dtuHmAddr, wrHmAddr, ++info); //read back activePowerLimit
            }
            else
            {
                if (rxlen > 0) ++info; //request next information
                request_info(dtuHmAddr, wrHmAddr, info);
            }
            init_receiver();
            ttx = time1ms;
            trx = time1ms;
            rxlen = 0;
        }

        if ((time1ms - trx) >= 4) //ervery 4ms
        {
            next_rx_channel();
            trx = time1ms;
        }

        unsigned int len;
        if (len = receive(buffer))
        {
            rxlen += len;
            if (info == DeviceResponse::INFO_CMD_ID)
            {
                device.set_response_data(buffer[9], &buffer[10]);
                std::cout << device.json().dump() << std::endl;
            }
            if (info == ConfigResponse::INFO_CMD_ID)
            {
                config.set_response_data(buffer[9], &buffer[10]);
                std::cout << config.json().dump() << std::endl;
            }
            if (info == StatusResponse::INFO_CMD_ID)
            {
                status.set_response_data(buffer[9], &buffer[10]);
                if (buffer[9] == 2)
                {
                    inverterStatus = status.json();
                    inverterStatus["pwrlimit"] = (double)config.active_power_limit() / 10;
                    // inverterStatus['serialno'] = wrSerial;
                    std::string s = inverterStatus.dump();
                    std::cout << s << std::endl;
                    eventDispatcher.send_event(std::move(s));
                }
            }
        }
        usleep(77);
    }
    return 0;
}

//Cmd (see ahoy InfoCmdType)
// InverterDevInform_All    = 1,  // 0x01
// SystemConfigPara         = 5,  // 0x05
// RealTimeRunData_Debug    = 11, // 0x0b
bool request_info(const uint8_t dtuHmAddr[4], const uint8_t wrHmAddr[4], uint8_t cmd)
{
    //setup template
    uint8_t req[] =
    {
        0x15,
        0x82, 0x92, 0x36, 0x75, //WR HM addr
        0x70, 0x53, 0x54, 0x53, //DTU HM addr
        0x80,
        0x01, //InverterDevInform_All (class InfoCommands(IntEnum))
        0x00,
        0x67, 0xE5, 0x64, 0x79, //Unixtime
        0x00, 0x00,
        0x00, 0x00, //2 byte alarm? / sequence?
        0x00, 0x00, 0x00, 0x00, //4 byte payload
        0x77, 0xDF, //MODBUS CRC (HI LO)
        0xDE //CRC8
    };
    //set WR HM addr
    req[1] = wrHmAddr[0];
    req[2] = wrHmAddr[1];
    req[3] = wrHmAddr[2];
    req[4] = wrHmAddr[3];
    //set DTU HM addr
    req[5] = dtuHmAddr[0];
    req[6] = dtuHmAddr[1];
    req[7] = dtuHmAddr[2];
    req[8] = dtuHmAddr[3];
    //which type of information (vgl. ahoy InfoCmdType)
    req[10] = cmd;
    //set Time
    const uint32_t utc = std::time(nullptr); // Aktuelle Zeit in UTC
    req[12] = (utc >> 24) & 0xFF;
    req[13] = (utc >> 16) & 0xFF;
    req[14] = (utc >> 8) & 0xFF;
    req[15] = (utc >> 0) & 0xFF;
    //modbus CRC
    const uint16_t crc16 = crc_calc_crc16_reflected(0xFFFF, &req[10], 14);
    req[24] = (crc16 >> 8) & 0xFF;
    req[25] = (crc16 >> 0) & 0xFF;
    //crc8
    req[26] = crc_calc_crc8(0, &req[0], 26);
    return transmit(req, sizeof(req));
}


//Cmd (see ahoy DevControlCmdType)
// ActivePowerContr        = 11, // 0x0b - in this case data is 0.1*percent of maximal power (1000 -> 100%)
bool set_active_power_limit(const uint8_t dtuHmAddr[4], const uint8_t wrHmAddr[4], uint16_t limit)
{
    //setup template
    uint8_t req[] =
    {
        0x51,
        0x82, 0x92, 0x36, 0x75, //WR HM addr
        0x70, 0x53, 0x54, 0x53, //DTU HM addr
        0x81,
        0x0B, // CMD = ActivePowerContr
        0x00,
        0x00, 0x00, //Power limit (0.1W), big endian
        0x00, //store persistent: 00=no, 01=yes
        0x01, //00=absolute value (0.1W), 01=relative value (0.1%)
        0x77, 0xDF, //MODBUS CRC (HI LO)
        0xDE //CRC8
    };
    //set WR HM addr
    req[1] = wrHmAddr[0];
    req[2] = wrHmAddr[1];
    req[3] = wrHmAddr[2];
    req[4] = wrHmAddr[3];
    //set DTU HM addr
    req[5] = dtuHmAddr[0];
    req[6] = dtuHmAddr[1];
    req[7] = dtuHmAddr[2];
    req[8] = dtuHmAddr[3];
    //data value
    req[12] = (limit >> 8) & 0xFF;
    req[13] = (limit >> 0) & 0xFF;
    //modbus CRC
    const uint16_t crc16 = crc_calc_crc16_reflected(0xFFFF, &req[10], 6);
    req[16] = (crc16 >> 8) & 0xFF;
    req[17] = (crc16 >> 0) & 0xFF;
    //crc8
    req[18] = crc_calc_crc8(0, &req[0], 18);
    return transmit(req, sizeof(req));
}


//Not tested yet!!!
//Cmd (see ahoy DevControlCmdType)
// Restart                 = 2,  // 0x02
bool restart_inverter(const uint8_t dtuHmAddr[4], const uint8_t wrHmAddr[4])
{
    //setup template
    uint8_t req[] =
    {
        0x51,
        0x82, 0x92, 0x36, 0x75, //WR HM addr
        0x70, 0x53, 0x54, 0x53, //DTU HM addr
        0x81,
        0x02, // CMD = Restart
        0x00,
        0x77, 0xDF, //MODBUS CRC (HI LO)
        0xDE //CRC8
    };
    //set WR HM addr
    req[1] = wrHmAddr[0];
    req[2] = wrHmAddr[1];
    req[3] = wrHmAddr[2];
    req[4] = wrHmAddr[3];
    //set DTU HM addr
    req[5] = dtuHmAddr[0];
    req[6] = dtuHmAddr[1];
    req[7] = dtuHmAddr[2];
    req[8] = dtuHmAddr[3];
    //modbus CRC
    const uint16_t crc16 = crc_calc_crc16_reflected(0xFFFF, &req[10], 2);
    req[12] = (crc16 >> 8) & 0xFF;
    req[13] = (crc16 >> 0) & 0xFF;
    //crc8
    req[14] = crc_calc_crc8(0, &req[0], 14);
    return transmit(req, sizeof(req));
}


//Transmit data from source to destination
//Return:
// - true, if an ACK was received
// - false, if no ack was received
bool transmit(const uint8_t * data, size_t len)
{
    uint8_t esbAddr[5] = { 0x01, 0x00, 0x00, 0x00, 0x00 };

    radio.stopListening(); // put radio in TX mode
    radio.setDataRate(RF24_250KBPS);
    esbAddr[1] = data[5];
    esbAddr[2] = data[6];
    esbAddr[3] = data[7];
    esbAddr[4] = data[8];
    radio.openReadingPipe(1, esbAddr); //set dtu esb addr
    esbAddr[1] = data[1];
    esbAddr[2] = data[2];
    esbAddr[3] = data[3];
    esbAddr[4] = data[4];
    radio.openWritingPipe(esbAddr); // set wr esb addr
    radio.setChannel(++txChannel);
    radio.setAutoAck(true);
    radio.setRetries(3, 15);
    radio.setCRCLength(RF24_CRC_16);
    radio.enableDynamicPayloads();
    radio.setPALevel(RF24_PA_MAX); // RF24_PA_MAX is default.

    if (verbose)
    {
        char buffer[128];
        printf("[%s] TX", get_timestamp(buffer));
        printf(" ch=%u", (uint8_t)txChannel);
        printf(" req=%u", (uint8_t)data[10]);
        printf(" data=\"%s\"\n", get_hexdump(buffer, data, len, ' '));
    }

    return radio.write(data, len);
}



void init_receiver()
{
    radio.setChannel(rxChannel);
    radio.setAutoAck(false);
    radio.setRetries(0, 0);
    radio.enableDynamicPayloads();
    radio.setCRCLength(RF24_CRC_16);
    radio.startListening(); // put radio in RX mode
}

void next_rx_channel()
{
    radio.stopListening();
    radio.setChannel(++rxChannel);
    radio.startListening();
}

unsigned int receive(uint8_t buffer[32])
{
    uint8_t pipe;
    if (radio.available(&pipe))
    {
        const uint8_t len = radio.getDynamicPayloadSize(); // get the size of the payload
        radio.read(buffer, len); // fetch payload from FIFO
        if (verbose)
        {
            char buf[128];
            printf("[%s] RX", get_timestamp(buf));
            printf(" ch=%u", (uint8_t)rxChannel);
            printf(" req=%u", (uint8_t)info);
            printf(" data=\"%s\"\n", get_hexdump(buf, buffer, len, ' '));
        }
        return len;
    }
    return 0;
}
