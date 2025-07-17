#include "ferm.h"

void serial_ferm_packet_print(const ferm_packet *packet) {
    if (packet == NULL) {
        Serial.println("Invalid FERM packet: NULL pointer.");
        return;
    }

    Serial.println("FERM Packet:");

    // Print flags
    Serial.print("  Flags: ");
    if (packet->header.is_sof) {
        Serial.print("SOF ");
    }
    if (packet->header.is_ack) {
        Serial.print("ACK ");
    }
    if (packet->header.is_fragment) {
        Serial.print("Fragment ");
    }
    if (packet->header.is_final) {
        Serial.print("Final ");
    }
    if (packet->header.is_restransmit) {
        Serial.print("Retransmit ");
    }
    Serial.println();

    // Upper Layer Protocol
    Serial.print("  Upper Layer Protocol: ");
    switch (packet->header.ulp) {
        case ULP_UART:
            Serial.println("UART");
            break;
        case ULP_SPI:
            Serial.println("SPI");
            break;
        case ULP_I2C:
            Serial.println("I2C");
            break;
        default:
            Serial.println("Unknown");
            break;
    }

    // Data length
    Serial.print("  Data Length: ");
    Serial.println(packet->header.data_len);

    // Checksum
    Serial.print("  Checksum: ");
    Serial.println(packet->header.checksum);

    // Payload
    if (packet->data != NULL) {
        Serial.print("  Data: ");
        for (uint8_t i = 0; i < packet->header.data_len; i++) {
            Serial.print("0x");
            if (((uint8_t *)packet->data)[i] < 0x10) {
                Serial.print("0"); // pad single digit hex
            }
            Serial.print(((uint8_t *)packet->data)[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    } else {
        Serial.println("  No data available.");
    }

    Serial.println(); // Extra newline for readability
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  ferm_packet packet;
  uint8_t data_len = 15;
  uint8_t payload1[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

  uint8_t flags = 0;
  ferm_set_flag(&flags, PACKET_FLAG_START);
  ferm_set_flag(&flags, PACKET_FLAG_FRAGMENT);
  ferm_packet_init(&packet, flags, ULP_UART, payload1, data_len);
  size_t max_size = sizeof(ferm_header_t) + packet.header.data_len + 1; // +1 for optional data CRC
  uint8_t *buffer = (uint8_t *)malloc(data_len + 3);      // Consider a fixed-size buffer pool or statically allocated buffers for robustness.
  size_t num_bytes = ferm_serialize_packet(&packet, buffer, data_len + 3); // Example serialization
  Serial.write(buffer, data_len + 3);
  free(buffer);

  uint8_t payload2[] = {16,17,18,19,20,21,22,23,24,25,26,27,28,29,30};
  flags = 0;
  ferm_set_flag(&flags, PACKET_FLAG_FRAGMENT);
  ferm_set_flag(&flags, PACKET_FLAG_FINAL);
  ferm_packet_init(&packet, flags, ULP_UART, payload2, data_len);
  max_size = sizeof(ferm_header_t) + packet.header.data_len + 1; // +1 for optional data CRC
  buffer = (uint8_t *)malloc(data_len + 3);
  num_bytes = ferm_serialize_packet(&packet, buffer, data_len + 3); // Example serialization
  Serial.write(buffer, data_len + 3);
  free(buffer);


  ferm_packet_free(&packet);
}

void loop() {
  // put your main code here, to run repeatedly:
  
}
