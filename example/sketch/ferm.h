#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// CRC-8 polynomial: x^8 + x^2 + x + 1 = 0x07
#define CRC8_POLY 0x07
#define CRC8_INIT 0x00


    typedef enum
    {
        PACKET_FLAG_NONE = 0b00000,      // No flags set
        PACKET_FLAG_START = 0b10000,     // Start of Frame (SOF) flag
        PACKET_FLAG_ACK = 0b01000,       // Urgent packet
        PACKET_FLAG_FRAGMENT = 0b00100,  // Packet is a fragment
        PACKET_FLAG_FINAL = 0b00010,     // Packet is the final fragment
        PACKET_FLAG_RETRANSMIT = 0b00001 // Packet is a retransmission
    } ferm_packet_flag;

    typedef enum
    {
        ULP_UART = 0b001,
        ULP_SPI = 0b010,
        ULP_I2C = 0b011
    } upper_layer_protocol;

    /**
     * Structure representing the header of a packet in the FERM protocol.
     * || SOF (1 bit) || TYPE (2 bits) || FLAGS (2 bits) || ULP (3 bits) || LENGTH (4 bits) || RESERVED (4 bits) || CHECKSUM (8 bits) ||
     * Total size: 3 bytes (1 byte control + 1 byte length + 1 byte checksum)
     */

    typedef struct __attribute__((packed))
    {
        union
        {
            struct
            {
                uint8_t is_sof : 1;
                uint8_t is_ack : 1;
                uint8_t is_fragment : 1;
                uint8_t is_final : 1;
                uint8_t is_restransmit : 1;
                uint8_t ulp : 3;
            };
            uint8_t control_byte; // Full control access to first byte
        };

        union
        {
            struct
            {
                uint8_t data_len : 4;
                uint8_t reserved : 4;
            };
            uint8_t length_byte;
        };

        uint8_t checksum;
    } ferm_header_t;

    /**
     * Structure representing a packet in the FERM protocol.
     *
     */
    typedef struct
    {
        ferm_header_t header; // Header containing metadata about the packet
        void *data;           // Pointer to the packet data - maxLen = 15 bytes
    } ferm_packet;

    void ferm_packet_init(ferm_packet *packet, uint8_t flags, upper_layer_protocol ulp, const void *data, size_t data_len);
    void ferm_packet_free(ferm_packet *packet);

    void ferm_packet_print(const ferm_packet *packet);

    uint8_t ferm_get_checksum(const uint8_t *data, size_t len);
    void ferm_set_flag(uint8_t* flag, ferm_packet_flag to_set);

    size_t ferm_serialize_packet(const ferm_packet *packet, uint8_t *buffer, size_t buffer_len);

#ifdef __cplusplus
}
#endif