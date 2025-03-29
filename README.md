# HOMICX - Hoymiles Microinverter Control

This tool is (inspired by [Ahoy's RPI Tools](https://github.com/lumapu/ahoy/tree/main/tools/rpi)) used to read measurement data from Hoymiles microinverter and control ist maximum power output (aka active power limit).

It is used to run on a Raspberry PI and requires a nRF24L01+ connected to PIs SPI interface (as described [here](https://tutorials-raspberrypi.de/funkkommunikation-zwischen-raspberry-pis-und-arduinos-2-4-ghz/)).

## Usage

```
Hoymiles Microinverter Control V0.1.0
 [--help] [--version] --serial=<SERIAL> [--poll=<INTERVAL>] --listen=<[IP:]PORT> [--serve=<PATH>] [--verbose]

  --help                    Print help and exit
  --version                 Print version and exit
  --serial=<SERIAL>         Serial number of microinverter
  --poll=<INTERVAL>         Poll inverter every INTERVAL seconds (default=5)
  --listen=<[IP:]PORT>      HTTP server listen address (default IP=0.0.0.0)
  --serve=<PATH>            Path to directory statically served by HTTP server
  --verbose                 Be verbose
```

Example:

```
build/homicx --serial 114182923675 --poll 2 --listen 8088 --serve html --verbose
```

Now open webbrowser on `http://<your-raspberry>:8088` and you will find a *swagger UI* to operate the REST API.

## Build 

### Prerequisits

First install nRF24L01+ library as described [here](https://nrf24.github.io/RF24/md_docs_2using__cmake.html). 
For short, on your raspberry pi, do the following and install the necessary dependencies and at least the library (network, mesh, etc is not required):

```
wget https://raw.githubusercontent.com/nRF24/.github/main/installer/install.sh
chmod +x install.sh
./install.sh
```

### Build

This should be easily done by:

```
mkdir build
cd build
cmake ..
make
```
