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
      ferm_packet reply;
      uint8_t *buffer;
      StatusCode status = ferm_get_packet(bufferRcvd, index, &recvd_packet);
      if (status == STATUS_SUCCESS)  {
        status = ferm_create_ack_packet(&reply, &buffer, ULP_UART);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
        if (status != STATUS_SUCCESS) {
          Serial.println("Failed to create ACK packet");
          index = 0;
          packet_len = -1;
          Serial.flush();
          continue;
        }
      }
      else if (status == STATUS_ERROR_CHECKSUM_MISMATCH){
        status = ferm_create_nack_packet(&reply, &buffer, ULP_UART);
        if (status != STATUS_SUCCESS) {
          Serial.println("Failed to create NACK packet");
          index = 0;
          packet_len = -1;
          Serial.flush();
          continue;
        }
        index = 0;
        packet_len = -1;
        Serial.flush();
        continue;
      }
      else {
        index = 0;
        packet_len = -1;
        Serial.flush();
        continue;
      }

      Serial.write(buffer, reply.header.data_len + HEADER_SIZE);
      Serial.flush();
      free(buffer);

      // Reset buffer for next packet
      index = 0;
      packet_len = -1;
    }
  }
}
