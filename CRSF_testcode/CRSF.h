#ifndef H_CRSF
#define H_CRSF

#include <Arduino.h>
#include "HardwareSerial.h"

#ifdef PLATFORM_ESP32
#include "esp32-hal-uart.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#endif

#define PACKED __attribute__((packed))

#define CRSF_RX_BAUDRATE 420000
#define CRSF_OPENTX_BAUDRATE 400000
#define CRSF_OPENTX_SLOW_BAUDRATE 115200 // Used for QX7 not supporting 400kbps
#define CRSF_NUM_CHANNELS 16
#define CRSF_CHANNEL_VALUE_MIN 172
#define CRSF_CHANNEL_VALUE_MID 992
#define CRSF_CHANNEL_VALUE_MAX 1811
#define CRSF_MAX_PACKET_LEN 64

#define CRSF_SYNC_BYTE 0xC8

#define RCframeLength 22             // length of the RC data packed bytes frame. 16 channels in 11 bits each.
#define LinkStatisticsFrameLength 10 // length of the RC data packed bytes frame. 16 channels in 11 bits each.

#define CRSF_PAYLOAD_SIZE_MAX 62
#define CRSF_FRAME_NOT_COUNTED_BYTES 2
#define CRSF_FRAME_SIZE(payload_size) ((payload_size) + 2) // See crsf_header_t.frame_size
#define CRSF_EXT_FRAME_SIZE(payload_size) (CRSF_FRAME_SIZE(payload_size) + 2)
#define CRSF_FRAME_SIZE_MAX (CRSF_PAYLOAD_SIZE_MAX + CRSF_FRAME_NOT_COUNTED_BYTES)

// Macros for big-endian (assume little endian host for now) etc
#define CRSF_DEC_U16(x) ((uint16_t)__builtin_bswap16(x))
#define CRSF_DEC_I16(x) ((int16_t)CRSF_DEC_U16(x))
#define CRSF_DEC_U24(x) (CRSF_DEC_U32((uint32_t)x << 8))
#define CRSF_DEC_U32(x) ((uint32_t)__builtin_bswap32(x))
#define CRSF_DEC_I32(x) ((int32_t)CRSF_DEC_U32(x))

//////////////////////////////////////////////////////////////

#define CRSF_MSP_REQ_PAYLOAD_SIZE 8
#define CRSF_MSP_RESP_PAYLOAD_SIZE 58
#define CRSF_MSP_MAX_PAYLOAD_SIZE (CRSF_MSP_REQ_PAYLOAD_SIZE > CRSF_MSP_RESP_PAYLOAD_SIZE ? CRSF_MSP_REQ_PAYLOAD_SIZE : CRSF_MSP_RESP_PAYLOAD_SIZE)

/* CRC8 implementation with polynom = x​7​+ x​6​+ x​4​+ x​2​+ x​0 ​(0xD5) */
static unsigned char crc8tab[256] = {
    0x00, 0xD5, 0x7F, 0xAA, 0xFE, 0x2B, 0x81, 0x54, 0x29, 0xFC, 0x56, 0x83, 0xD7, 0x02, 0xA8, 0x7D,
    0x52, 0x87, 0x2D, 0xF8, 0xAC, 0x79, 0xD3, 0x06, 0x7B, 0xAE, 0x04, 0xD1, 0x85, 0x50, 0xFA, 0x2F,
    0xA4, 0x71, 0xDB, 0x0E, 0x5A, 0x8F, 0x25, 0xF0, 0x8D, 0x58, 0xF2, 0x27, 0x73, 0xA6, 0x0C, 0xD9,
    0xF6, 0x23, 0x89, 0x5C, 0x08, 0xDD, 0x77, 0xA2, 0xDF, 0x0A, 0xA0, 0x75, 0x21, 0xF4, 0x5E, 0x8B,
    0x9D, 0x48, 0xE2, 0x37, 0x63, 0xB6, 0x1C, 0xC9, 0xB4, 0x61, 0xCB, 0x1E, 0x4A, 0x9F, 0x35, 0xE0,
    0xCF, 0x1A, 0xB0, 0x65, 0x31, 0xE4, 0x4E, 0x9B, 0xE6, 0x33, 0x99, 0x4C, 0x18, 0xCD, 0x67, 0xB2,
    0x39, 0xEC, 0x46, 0x93, 0xC7, 0x12, 0xB8, 0x6D, 0x10, 0xC5, 0x6F, 0xBA, 0xEE, 0x3B, 0x91, 0x44,
    0x6B, 0xBE, 0x14, 0xC1, 0x95, 0x40, 0xEA, 0x3F, 0x42, 0x97, 0x3D, 0xE8, 0xBC, 0x69, 0xC3, 0x16,
    0xEF, 0x3A, 0x90, 0x45, 0x11, 0xC4, 0x6E, 0xBB, 0xC6, 0x13, 0xB9, 0x6C, 0x38, 0xED, 0x47, 0x92,
    0xBD, 0x68, 0xC2, 0x17, 0x43, 0x96, 0x3C, 0xE9, 0x94, 0x41, 0xEB, 0x3E, 0x6A, 0xBF, 0x15, 0xC0,
    0x4B, 0x9E, 0x34, 0xE1, 0xB5, 0x60, 0xCA, 0x1F, 0x62, 0xB7, 0x1D, 0xC8, 0x9C, 0x49, 0xE3, 0x36,
    0x19, 0xCC, 0x66, 0xB3, 0xE7, 0x32, 0x98, 0x4D, 0x30, 0xE5, 0x4F, 0x9A, 0xCE, 0x1B, 0xB1, 0x64,
    0x72, 0xA7, 0x0D, 0xD8, 0x8C, 0x59, 0xF3, 0x26, 0x5B, 0x8E, 0x24, 0xF1, 0xA5, 0x70, 0xDA, 0x0F,
    0x20, 0xF5, 0x5F, 0x8A, 0xDE, 0x0B, 0xA1, 0x74, 0x09, 0xDC, 0x76, 0xA3, 0xF7, 0x22, 0x88, 0x5D,
    0xD6, 0x03, 0xA9, 0x7C, 0x28, 0xFD, 0x57, 0x82, 0xFF, 0x2A, 0x80, 0x55, 0x01, 0xD4, 0x7E, 0xAB,
    0x84, 0x51, 0xFB, 0x2E, 0x7A, 0xAF, 0x05, 0xD0, 0xAD, 0x78, 0xD2, 0x07, 0x53, 0x86, 0x2C, 0xF9};

typedef enum
{
    CRSF_FRAMETYPE_GPS = 0x02,
    CRSF_FRAMETYPE_BATTERY_SENSOR = 0x08,
    CRSF_FRAMETYPE_LINK_STATISTICS = 0x14,
    CRSF_FRAMETYPE_RC_CHANNELS_PACKED = 0x16,
    CRSF_FRAMETYPE_ATTITUDE = 0x1E,
    CRSF_FRAMETYPE_FLIGHT_MODE = 0x21,
    // Extended Header Frames, range: 0x28 to 0x96
    CRSF_FRAMETYPE_DEVICE_PING = 0x28,
    CRSF_FRAMETYPE_DEVICE_INFO = 0x29,
    CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY = 0x2B,
    CRSF_FRAMETYPE_PARAMETER_READ = 0x2C,
    CRSF_FRAMETYPE_PARAMETER_WRITE = 0x2D,
    CRSF_FRAMETYPE_COMMAND = 0x32,
    // MSP commands
    CRSF_FRAMETYPE_MSP_REQ = 0x7A,   // response request using msp sequence as command
    CRSF_FRAMETYPE_MSP_RESP = 0x7B,  // reply with 58 byte chunked binary
    CRSF_FRAMETYPE_MSP_WRITE = 0x7C, // write with 8 byte chunked binary (OpenTX outbound telemetry buffer limit)
} crsf_frame_type_e;

typedef enum
{
    CRSF_ADDRESS_BROADCAST = 0x00,
    CRSF_ADDRESS_USB = 0x10,
    CRSF_ADDRESS_TBS_CORE_PNP_PRO = 0x80,
    CRSF_ADDRESS_RESERVED1 = 0x8A,
    CRSF_ADDRESS_CURRENT_SENSOR = 0xC0,
    CRSF_ADDRESS_GPS = 0xC2,
    CRSF_ADDRESS_TBS_BLACKBOX = 0xC4,
    CRSF_ADDRESS_FLIGHT_CONTROLLER = 0xC8,
    CRSF_ADDRESS_RESERVED2 = 0xCA,
    CRSF_ADDRESS_RACE_TAG = 0xCC,
    CRSF_ADDRESS_RADIO_TRANSMITTER = 0xEA,
    CRSF_ADDRESS_CRSF_RECEIVER = 0xEC,
    CRSF_ADDRESS_CRSF_TRANSMITTER = 0xEE,
} crsf_addr_e;

//typedef struct crsf_addr_e asas;

typedef enum
{
    CRSF_UINT8 = 0,
    CRSF_INT8 = 1,
    CRSF_UINT16 = 2,
    CRSF_INT16 = 3,
    CRSF_UINT32 = 4,
    CRSF_INT32 = 5,
    CRSF_UINT64 = 6,
    CRSF_INT64 = 7,
    CRSF_FLOAT = 8,
    CRSF_TEXT_SELECTION = 9,
    CRSF_STRING = 10,
    CRSF_FOLDER = 11,
    CRSF_INFO = 12,
    CRSF_COMMAND = 13,
    CRSF_VTX = 15,
    CRSF_OUT_OF_RANGE = 127,
} crsf_value_type_e;

typedef struct crsf_header_s
{
    uint8_t device_addr; // from crsf_addr_e
    uint8_t frame_size;  // counts size after this byte, so it must be the payload size + 2 (type and crc)
    uint8_t type;        // from crsf_frame_type_e
} PACKED crsf_header_t;

// Used by extended header frames (type in range 0x28 to 0x96)
typedef struct crsf_ext_header_s
{
    // Common header fields, see crsf_header_t
    uint8_t device_addr;
    uint8_t frame_size;
    uint8_t type;
    // Extended fields
    uint8_t dest_addr;
    uint8_t orig_addr;
} PACKED crsf_ext_header_t;

typedef struct crsf_channels_s
{
    unsigned ch0 : 11;
    unsigned ch1 : 11;
    unsigned ch2 : 11;
    unsigned ch3 : 11;
    unsigned ch4 : 11;
    unsigned ch5 : 11;
    unsigned ch6 : 11;
    unsigned ch7 : 11;
    unsigned ch8 : 11;
    unsigned ch9 : 11;
    unsigned ch10 : 11;
    unsigned ch11 : 11;
    unsigned ch12 : 11;
    unsigned ch13 : 11;
    unsigned ch14 : 11;
    unsigned ch15 : 11;
} PACKED crsf_channels_t;

typedef struct crsf_channels_s crsf_channels_t;

/*
 * 0x14 Link statistics
 * Payload:
 *
 * uint8_t Uplink RSSI Ant. 1 ( dBm * -1 )
 * uint8_t Uplink RSSI Ant. 2 ( dBm * -1 )
 * uint8_t Uplink Package success rate / Link quality ( % )
 * int8_t Uplink SNR ( db )
 * uint8_t Diversity active antenna ( enum ant. 1 = 0, ant. 2 )
 * uint8_t RF Mode ( enum 4fps = 0 , 50fps, 150hz)
 * uint8_t Uplink TX Power ( enum 0mW = 0, 10mW, 25 mW, 100 mW, 500 mW, 1000 mW, 2000mW )
 * uint8_t Downlink RSSI ( dBm * -1 )
 * uint8_t Downlink package success rate / Link quality ( % )
 * int8_t Downlink SNR ( db )
 * Uplink is the connection from the ground to the UAV and downlink the opposite direction.
 */

typedef struct crsfPayloadLinkstatistics_s
{
    uint8_t uplink_RSSI_1;
    uint8_t uplink_RSSI_2;
    uint8_t uplink_Link_quality;
    int8_t uplink_SNR;
    uint8_t active_antenna;
    uint8_t rf_Mode;
    uint8_t uplink_TX_Power;
    uint8_t downlink_RSSI;
    uint8_t downlink_Link_quality;
    int8_t downlink_SNR;
} crsfLinkStatistics_t;

typedef struct crsfPayloadLinkstatistics_s crsfLinkStatistics_t;

/////inline and utility functions//////

//static uint16_t ICACHE_RAM_ATTR fmap(uint16_t x, float in_min, float in_max, float out_min, float out_max) { return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min; };
static uint16_t ICACHE_RAM_ATTR fmap(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max) { return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min; };

static inline uint16_t ICACHE_RAM_ATTR CRSF_to_US(uint16_t Val) { return round(fmap(Val, 172.0, 1811.0, 988.0, 2012.0)); };
static inline uint16_t ICACHE_RAM_ATTR UINT10_to_CRSF(uint16_t Val) { return round(fmap(Val, 0.0, 1024.0, 172.0, 1811.0)); };

static inline uint16_t ICACHE_RAM_ATTR SWITCH3b_to_CRSF(uint16_t Val) { return round(map(Val, 0, 7, 188, 1795)); };

static inline uint8_t ICACHE_RAM_ATTR CRSF_to_BIT(uint16_t Val)
{
    if (Val > 1000)
        return 1;
    else
        return 0;
};
static inline uint16_t ICACHE_RAM_ATTR BIT_to_CRSF(uint8_t Val)
{
    if (Val)
        return 1795;
    else
        return 188;
};

static inline uint16_t ICACHE_RAM_ATTR CRSF_to_UINT10(uint16_t Val) { return round(fmap(Val, 172.0, 1811.0, 0.0, 1023.0)); };
static inline uint16_t ICACHE_RAM_ATTR UINT_to_CRSF(uint16_t Val);

static inline uint8_t ICACHE_RAM_ATTR CalcCRC(volatile uint8_t *data, int length);
static inline uint8_t ICACHE_RAM_ATTR CalcCRC(uint8_t *data, int length);

uint8_t ICACHE_RAM_ATTR CalcCRC(volatile uint8_t *data, int length)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < length; i++)
    {
        crc = crc8tab[crc ^ *data++];
    }
    return crc;
}

uint8_t ICACHE_RAM_ATTR CalcCRC(uint8_t *data, int length)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < length; i++)
    {
        crc = crc8tab[crc ^ *data++];
    }
    return crc;
}

class CRSF
{

public:
#ifdef PLATFORM_ESP8266
    //CRSF(HardwareSerial& serial);
    CRSF(Stream *dev) : _dev(dev) {}
    CRSF(Stream &dev) : _dev(&dev) {}

    void InitSerial()
    {
        _dev->println("I am ready");
    }

#endif
    static HardwareSerial Port;
    //static Stream *Port;

    static volatile uint16_t ChannelDataIn[16];
    static volatile uint16_t ChannelDataInPrev[16]; // Contains the previous RC channel data
    static volatile uint16_t ChannelDataOut[16];

    static void (*RCdataCallback1)(); //function pointer for new RC data callback
    static void (*RCdataCallback2)(); //function pointer for new RC data callback

    static void (*disconnected)();
    static void (*connected)();

    static void (*RecvParameterUpdate)();

    static volatile uint8_t ParameterUpdateData[2];

    static bool firstboot;

    static bool CRSFstate;
    static bool CRSFstatePrev;

    static uint8_t CSFR_TXpin_Module;
    static uint8_t CSFR_RXpin_Module;

    static uint8_t CSFR_TXpin_Recv;
    static uint8_t CSFR_RXpin_Recv;

    /////Variables/////

    static volatile crsf_channels_s PackedRCdataOut;            // RC data in packed format for output.
    static volatile crsfPayloadLinkstatistics_s LinkStatistics; // Link Statisitics Stored as Struct

    static void Begin(); //setup timers etc

    static void ICACHE_RAM_ATTR duplex_set_RX();
    static void ICACHE_RAM_ATTR duplex_set_TX();
    static void ICACHE_RAM_ATTR duplex_set_HIGHZ();
    static void ICACHE_RAM_ATTR FlushSerial();
    static void GetSYNC(); //SYNC to incomming data

#ifdef PLATFORM_ESP32
    static void ICACHE_RAM_ATTR ESP32uartTask(void *pvParameters);
#else
    static void ICACHE_RAM_ATTR ESP8266ReadUart();
#endif

    void ICACHE_RAM_ATTR sendRCFrameToFC();
    void ICACHE_RAM_ATTR sendLinkStatisticsToFC();
    void ICACHE_RAM_ATTR sendLinkStatisticsToTX();

    //static void BuildRCPacket(crsf_addr_e addr = CRSF_ADDRESS_FLIGHT_CONTROLLER); //build packet to send to the FC

    static void ICACHE_RAM_ATTR SerialISR();
    static void ICACHE_RAM_ATTR ProcessPacket();
    static void ICACHE_RAM_ATTR GetChannelDataIn();

    static void inline nullCallback(void);

    //static uint16_t ICACHE_RAM_ATTR CRSF_to_US(uint16_t Datain) { return (0.62477120195241f * Datain) + 881; };

private:
    Stream *_dev;

    static volatile uint8_t SerialInPacketLen;   // length of the CRSF packet as measured
    static volatile uint8_t SerialInPacketPtr;   // index where we are reading/writing
    static volatile uint8_t SerialInBuffer[100]; // max 64 bytes for CRSF packet serial buffer

    static volatile uint8_t CRSFoutBuffer[CRSF_MAX_PACKET_LEN + 1]; //index 0 hold the length of the datapacket

    static volatile bool ignoreSerialData; //since we get a copy of the serial data use this flag to know when to ignore it
    static volatile bool CRSFframeActive;  //since we get a copy of the serial data use this flag to know when to ignore it
};

//float fmap(float x, float in_min, float in_max, float out_min, float out_max);

#endif
