#include "ferm.h"

static uint8_t bufferRcvd[MAX_PACKET_SIZE];
static uint8_t index = 0;
static int packet_len = -1;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.begin(9600);
  memset(bufferRcvd, 0, sizeof(bufferRcvd)); 
}

void loop() {
  while (Serial.available()) {
    bufferRcvd[index++] = Serial.read();

    if (index == 3) {
      packet_len = ferm_get_packet_len(bufferRcvd);
    }

    // Overflow guard
    if (index >= MAX_PACKET_SIZE) {
      Serial.println("Buffer overflow, resetting");
      index = 0;
      packet_len = -1;
      memset(bufferRcvd, 0, sizeof(bufferRcvd));
    }

    if (packet_len != -1 && index >= packet_len) {
      ferm_packet recvd_packet;
      StatusCode status = ferm_get_packet(bufferRcvd, index, &recvd_packet);
      if (status != STATUS_SUCCESS) {
        Serial.println("Failed to get packet");
        index = 0;
        packet_len = -1;
        continue;
      }

      // send ack
      ferm_packet ack;
      uint8_t *buffer;
      status = ferm_create_ack_packet(&ack, &buffer, ULP_UART);
      if (status != STATUS_SUCCESS) {
        Serial.println("Failed to create ACK packet");
        index = 0;
        packet_len = -1;
        continue;
      }

      Serial.write(buffer, ack.header.data_len + HEADER_SIZE);
      Serial.flush();
      free(buffer);

      // Reset buffer for next packet
      index = 0;
      packet_len = -1;
    }
  }
}
