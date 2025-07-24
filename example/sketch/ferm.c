#include "ferm.h"

StatusCode ferm_packet_init(ferm_packet **packets, uint8_t **buffer, uint8_t flags, upper_layer_protocol ulp, const void *data, int data_len, int *total_size) {
    if (packets == NULL || data == NULL) {
        return STATUS_ERROR_INVALID_ARGUMENT; // Invalid parameters
    }

    size_t num_packets = (data_len + MAX_DATA_LENGTH - 1) / MAX_DATA_LENGTH;
    *packets = (ferm_packet*)malloc(num_packets*sizeof(ferm_packet));
    if (*packets == NULL) {
        return STATUS_ERROR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < num_packets; i++) {
        // Initialize the header
        ferm_packet *pkt = &((*packets)[i]);
        if (i == 0) 
            pkt->header.is_sof = 1;
        else 
            pkt->header.is_sof = 0;
        if (num_packets > 1)
            pkt->header.is_fragment = 1;
        else
            pkt->header.is_fragment = 0;
        if (i*MAX_DATA_LENGTH + MAX_DATA_LENGTH >= data_len) 
            pkt->header.is_final = 1;
        else 
            pkt->header.is_final = 0;
        pkt->header.is_ack = (flags & PACKET_FLAG_ACK) ? 1 : 0;
        pkt->header.is_restransmit = (flags & PACKET_FLAG_RETRANSMIT) ? 1 : 0;
        pkt->header.ulp = ulp;
        // pkt->header.reserved = 0; // Reserved bits set to 0
        if (i*MAX_DATA_LENGTH + MAX_DATA_LENGTH >= data_len)
            pkt->header.data_len = data_len - i* MAX_DATA_LENGTH;
        else
            pkt->header.data_len = MAX_DATA_LENGTH; 

        pkt->data = malloc(pkt->header.data_len);
        if (pkt->data != NULL) {
            memcpy(pkt->data, (uint8_t *)data + i * MAX_DATA_LENGTH, pkt->header.data_len);
        }

        uint8_t temp_buf[MAX_PACKET_SIZE];
        temp_buf[0] = pkt->header.control_byte;
        temp_buf[1] = pkt->header.data_len & 0x0F; // Lower 4 bits for data length
        memcpy(&temp_buf[2], (uint8_t *)data + i * MAX_DATA_LENGTH, pkt->header.data_len); // without the checksum byte
        pkt->header.checksum = ferm_get_checksum(temp_buf, 2 + pkt->header.data_len);  
        *total_size += 3 + pkt->header.data_len;      
    }

    // Serialize the packet
    *buffer = (uint8_t *)malloc(*total_size);
    if (buffer == NULL) {
        // free(packet->data); 
        // packet->data = NULL;
        return STATUS_ERROR_OUT_OF_MEMORY; // Memory allocation failed
    }
    StatusCode ret = ferm_serialize_packet(packets, *buffer, num_packets);
    return ret;
}

StatusCode ferm_serialize_packet(ferm_packet **packets, uint8_t *buffer, int num_packets) {
    if (packets == NULL || buffer == NULL) {
        return STATUS_ERROR_INVALID_ARGUMENT;
    }
    int index = 0;

    for (int i=0; i<num_packets; i++) {
        ferm_packet *pkt = &((*packets)[i]);
        // Serialize header 
        buffer[index] = pkt->header.control_byte; 
        buffer[index + 1] = pkt->header.data_len; 
        buffer[index + 2] = pkt->header.checksum;

        // Copy payload
        memcpy(&buffer[index + 3], pkt->data, pkt->header.data_len);
        index += 3 + pkt->header.data_len;
    }

    return STATUS_SUCCESS;
}

StatusCode ferm_packet_free(ferm_packet *packet) {
    if (packet != NULL && packet->data != NULL) {
        free(packet->data);
        packet->data = NULL;
        return STATUS_SUCCESS; 
    }
    return STATUS_ERROR_INVALID_ARGUMENT;
}

void ferm_set_flag(uint8_t* flag, ferm_packet_flag to_set) {
    *flag |= to_set; 
}

void ferm_unset_flag(uint8_t* flag, ferm_packet_flag to_set) {
    *flag &= ~to_set; 
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



int ferm_check_ack(const ferm_packet packet) {
    if (packet.header.is_ack) {
        return 1; 
    }
    return 0; 
}

StatusCode ferm_get_packet(const uint8_t *bufferRcvd, size_t index, ferm_packet *packet) {
    if (index < 3) {
        packet->header.data_len = 0;
        packet->data = NULL;
        return STATUS_ERROR_INVALID_ARGUMENT;
    }

    // Initialize the packet header
    uint8_t flags = (bufferRcvd[0] & 0xF8) >> 3; // Upper 5 bits of control byte
    packet->header.is_sof = (flags & PACKET_FLAG_START) != 0 ? 1 : 0;
    packet->header.is_ack = (flags & PACKET_FLAG_ACK) != 0 ? 1 : 0;
    packet->header.is_fragment = (flags & PACKET_FLAG_FRAGMENT) != 0 ? 1 : 0;
    packet->header.is_final = (flags & PACKET_FLAG_FINAL) != 0 ? 1 : 0;
    packet->header.is_restransmit = (flags & PACKET_FLAG_RETRANSMIT) != 0 ? 1 : 0;
    packet->header.ulp = bufferRcvd[0] & 0x07;

    packet->header.data_len = bufferRcvd[1] & 0x0F; // Lower 4 bits of length byte
    packet->header.checksum = bufferRcvd[2]; // Checksum byte

    if (packet->header.data_len > 15 || index < (HEADER_SIZE + packet->header.data_len)) {
        // Invalid data length or not enough data
        packet->header.data_len = 0;
        packet->data = NULL;
        return STATUS_ERROR_DATA_LENGTH_INVALID;
    }
    // Allocate memory for the data
    packet->data = malloc(packet->header.data_len); 
    if (packet->data != NULL) {
        memcpy(packet->data, &bufferRcvd[3], packet->header.data_len);
    } else {
        packet->header.data_len = 0; 
        return STATUS_ERROR_OUT_OF_MEMORY;
    }

    // Validate checksum
    uint8_t temp_buf[MAX_PACKET_SIZE]; 
    temp_buf[0] = (flags << 3) | packet->header.ulp; // Upper 5 bits for flags, lower 3 bits for ULP
    temp_buf[1] = packet->header.data_len & 0x0F; // Lower 4 bits for data length
    memcpy(&temp_buf[2], packet->data, packet->header.data_len);
    uint8_t calculated_checksum = ferm_get_checksum(temp_buf, 2 + packet->header.data_len);
    if (calculated_checksum != packet->header.checksum) {
        free(packet->data);
        packet->data = NULL;
        packet->header.data_len = 0;
        return STATUS_ERROR_CHECKSUM_MISMATCH;
    }
    return STATUS_SUCCESS;
}


StatusCode ferm_create_ack_packet(ferm_packet *packet, uint8_t **buffer, upper_layer_protocol ulp) {
    uint8_t flags = 0;
    ferm_set_flag(&flags, PACKET_FLAG_ACK);
    unsigned char data = (unsigned char)(rand() % 256);
    StatusCode status = ferm_packet_init(&packet, buffer, flags, ulp, &data, 1, NULL);
    if (status != STATUS_SUCCESS) {
        return status; 
    }
    return STATUS_SUCCESS;
}

StatusCode ferm_create_nack_packet(ferm_packet *packet, uint8_t **buffer, upper_layer_protocol ulp) {
    uint8_t flags = 0;
    ferm_unset_flag(&flags, PACKET_FLAG_ACK);
    unsigned char data = (unsigned char)(rand() % 256);
    StatusCode status = ferm_packet_init(&packet, buffer, flags, ulp, &data, 1, NULL);
    if (status != STATUS_SUCCESS) {
        return status; 
    }
    return STATUS_SUCCESS;
}

int ferm_get_packet_len(const uint8_t *bufferRcvd) {
    if (bufferRcvd == NULL) {
        return -1; 
    }
    return (bufferRcvd[1] & 0x0F) + HEADER_SIZE; // Length byte + header size
}