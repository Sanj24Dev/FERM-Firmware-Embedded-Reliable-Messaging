#include "ferm.h"

void ferm_packet_init(ferm_packet *packet, uint8_t flags, upper_layer_protocol ulp, const void *data, size_t data_len) {
    if (packet == NULL || data == NULL || data_len > 15) {
        return; // Invalid parameters
    }

    // Initialize the header
    packet->header.is_sof = (flags & PACKET_FLAG_START) ? 1 : 0;
    packet->header.is_ack = (flags & PACKET_FLAG_ACK) ? 1 : 0;
    packet->header.is_fragment = (flags & PACKET_FLAG_FRAGMENT) ? 1 : 0;
    packet->header.is_final = (flags & PACKET_FLAG_FINAL) ? 1 : 0;
    packet->header.is_restransmit = (flags & PACKET_FLAG_RETRANSMIT) ? 1 : 0;
    packet->header.ulp = ulp;
    // packet->header.reserved = 0; // Reserved bits set to 0
    packet->header.data_len = data_len; // Limit to 15 bytes

    uint8_t temp_buf[32];  // Make sure this is large enough: header_size + data_len
    temp_buf[0] = (flags << 3) | (ulp & 0x07); // Upper 5 bits for flags, lower 3 bits for ULP
    temp_buf[1] = data_len & 0x0F; // Lower 4 bits for data length
    int header_len = 2; // without the checksum byte
    memcpy(&temp_buf[header_len], data, data_len);
    packet->header.checksum = ferm_get_checksum(temp_buf, header_len + data_len);

    // Allocate memory for the data
    packet->data = malloc(packet->header.data_len);
    if (packet->data != NULL) {
        memcpy(packet->data, data, packet->header.data_len);
    }
}

void ferm_packet_free(ferm_packet *packet) {
    if (packet != NULL && packet->data != NULL) {
        free(packet->data);
        packet->data = NULL; // Avoid dangling pointer
    }
}

void ferm_set_flag(uint8_t* flag, ferm_packet_flag to_set) {
    *flag |= to_set; 
}

void ferm_packet_print(const ferm_packet *packet) {
    if (packet == NULL) {
        return; // Invalid packet
    }

    printf("FERM Packet:\n");
    printf("    Flags: ");
    if (packet->header.is_sof) {
        printf("Start of Frame (SOF) ");
    }
    if (packet->header.is_ack) {
        printf("ACK ");
    }
    if (packet->header.is_fragment) {
        printf("Fragment ");
    }
    if (packet->header.is_final) {
        printf("Final ");
    }
    if (packet->header.is_restransmit) {
        printf("Retransmit ");
    }
    printf("\n    Upper Layer Protocol: ");
    if (packet->header.ulp == ULP_UART) {
        printf("UART\n");
    } else if (packet->header.ulp == ULP_SPI) {
        printf("SPI\n");
    } else if (packet->header.ulp == ULP_I2C) {
        printf("I2C\n");
    } else {
        printf("Unknown protocol\n");
    }
    printf("  Data Length: %d\n", packet->header.data_len);
    printf("  Checksum: %d\n", packet->header.checksum);

    if (packet->data != NULL) {
        printf("  Data: ");
        for (uint8_t i = 0; i < packet->header.data_len; i++) {
            printf("%d ", ((uint8_t *)packet->data)[i]);
        }
        printf("\n");
    } else {
        printf("  No data available.\n");
    }
}


uint8_t ferm_get_checksum(const uint8_t *data, size_t len) {
    uint8_t crc = CRC8_INIT;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ CRC8_POLY;
            else
                crc <<= 1;
        }
    }
    return crc;
}

size_t ferm_serialize_packet(const ferm_packet *packet, uint8_t *buffer, size_t buffer_len) {
    if (packet == NULL || buffer == NULL || buffer_len < 2 + packet->header.data_len) {
        return 0;
    }

    // Serialize header 
    buffer[0] = 0;
    if (packet->header.is_sof) {
        buffer[0] |= PACKET_FLAG_START;
    }
    if (packet->header.is_ack) {
        buffer[0] |= PACKET_FLAG_ACK;
    }
    if (packet->header.is_fragment) {
        buffer[0] |= PACKET_FLAG_FRAGMENT;
    }
    if (packet->header.is_final) {
        buffer[0] |= PACKET_FLAG_FINAL;
    }
    if (packet->header.is_restransmit) {
        buffer[0] |= PACKET_FLAG_RETRANSMIT;
    }
    buffer[0] = (buffer[0] << 3) | packet->header.ulp; // ULP occupies bits 3-5
    buffer[1] = packet->header.data_len; // Length byte
    buffer[2] = packet->header.checksum;

    // Copy payload
    memcpy(&buffer[3], packet->data, packet->header.data_len);

    return 3 + packet->header.data_len;
}

int ferm_check_ack(const uint8_t *bufferRcvd, size_t index) {
    int retVal = 0;
    if (index >= 3) {  
      uint8_t data_len = bufferRcvd[1] & 0x0F;  // Lower 4 bits of length byte
      uint8_t total_len = 3 + data_len;     // Full packet = header + data
      
      // Read flags
      uint8_t flags = ( bufferRcvd[0] & 0xF8 ) >> 3; // Upper 5 bits of control byte
      retVal = (flags & PACKET_FLAG_ACK) != 0 ? 1 : 0;
    }
    return retVal;
}