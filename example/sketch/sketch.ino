#include "ferm.h"

static uint8_t bufferRcvd[18];
unsigned long timeout = 5000; // 5 seconds timeout

void setup() {
  // put your setup code here, to run once:
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  uint8_t data_len = 24;
  uint8_t payload1[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24}; 

  // FERM 
  StatusCode status;
  uint8_t flags = 0;
  ferm_packet *packet;
  uint8_t *buffer;
  int total_size = 0;
  status = ferm_packet_init(&packet, &buffer, flags, ULP_UART, payload1, data_len, &total_size);
  if (status != STATUS_SUCCESS) {
    Serial.println("Failed to initialize packet");
    // status = ferm_packet_free(&packet);
    free(buffer);
    return;
  }
  //

  Serial.write(buffer, total_size);
  Serial.flush();
  free(buffer);
  
  static uint8_t index = 0;
  bool is_ack = false;
  unsigned long startTime = millis();
  while ((millis() - startTime) < timeout) {
    while (Serial.available()) {
      bufferRcvd[index++] = Serial.read();
    }

    // FERM
    if (index >= HEADER_SIZE) {
      ferm_packet recvd_packet;
      status = ferm_get_packet(bufferRcvd, index, &recvd_packet);
      is_ack = ferm_check_ack(recvd_packet);
      if (is_ack) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(3000);
        digitalWrite(LED_BUILTIN, LOW);
        index = 0;
        is_ack = false;
        return; 
      } else { 
        Serial.println("NACK received, retransmit");
        index = 0;
        break;
      }
    }
    //
  }
  delay(1000);
}
