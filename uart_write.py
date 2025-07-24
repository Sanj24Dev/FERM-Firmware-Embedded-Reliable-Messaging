import serial
import time
import random

SERIAL_PORT = "COM8"
BAUD_RATE = 9600

# Flag masks
PACKET_FLAG_START = 0b10000
PACKET_FLAG_ACK = 0b01000
PACKET_FLAG_FRAGMENT = 0b00100
PACKET_FLAG_FINAL = 0b00010
PACKET_FLAG_RETRANSMIT = 0b00001

# Upper Layer Protocols
ULP_UART = 0b001
ULP_SPI = 0b010
ULP_I2C = 0b011

def crc8(data: bytes, poly=0x07, init=0x00) -> int:
    crc = init
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ poly) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc

def parse_packet(buffer):
    if len(buffer) < 3:
        raise ValueError("Buffer too short")

    control_byte = buffer[0]
    length_byte = buffer[1]
    checksum = buffer[2]
    payload = buffer[3:]

    flags = ( control_byte & 0b11111000 ) >> 3  # upper 5 bits
    ulp   = ( control_byte & 0b00000111 )  # lower 3 bits

    is_sof        = bool(flags & PACKET_FLAG_START)
    is_ack        = bool(flags & PACKET_FLAG_ACK)
    is_fragment   = bool(flags & PACKET_FLAG_FRAGMENT)
    is_final      = bool(flags & PACKET_FLAG_FINAL)
    is_retransmit = bool(flags & PACKET_FLAG_RETRANSMIT)

    data_len = length_byte & 0x0F
    

    # print("Parsed Packet:")
    # print(f"  is_sof       : {is_sof}")
    # print(f"  is_ack       : {is_ack}")
    # print(f"  is_fragment  : {is_fragment}")
    # print(f"  is_final     : {is_final}")
    # print(f"  is_retransmit: {is_retransmit}")
    # print(f"  ULP field    : {ulp}")
    # print(f"  Data Length  : {data_len}")
    # print(f"  Checksum     : {checksum}")
    # print("  Payload      :", [int(b) for b in payload])

    return {
        "is_sof": is_sof,
        "is_ack": is_ack,
        "is_fragment": is_fragment,
        "is_final": is_final,
        "is_retransmit": is_retransmit,
        "ulp": ulp,
        "data_len": data_len,
        "checksum": checksum,
        "payload": payload
    }

def build_ferm_packet(flags: int, ulp: int, payload: bytes) -> bytes:
    if len(payload) > 15:
        raise ValueError("Payload too long. Max length is 15 bytes.")

    control_byte = ((flags & 0x1F) << 3) | (ulp & 0x07)
    length_byte = (len(payload) & 0x0F)

    # Compute checksum over control + length + payload
    to_checksum = bytes([control_byte, length_byte]) + payload
    checksum = crc8(to_checksum)

    # Assemble the final packet
    return bytes([control_byte, length_byte, checksum]) + payload

def send_packet_over_uart(port, baudrate, packet: bytes):
    try:
        with serial.Serial(port, baudrate, timeout=1) as ser:
            ser.write(packet)
            print(f"Sent {len(packet)} bytes:", packet.hex())
    except serial.SerialException as e:
        print("Serial error:", e)


def main():
    i = 1
    try:
        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
            while True: 
                print(f"Opened {SERIAL_PORT} at {BAUD_RATE} baud...")
                time.sleep(2)  # Give the serial port some time to initialize

                # Step 1: Prepare and send data packet
                data = bytes([1,2,3,4,5,6,7,8,9])
                flags = PACKET_FLAG_START | PACKET_FLAG_FINAL
                ulp = ULP_UART
                packet = build_ferm_packet(flags, ulp, data)

                print("Sending data packet " + str(i) + " ...")
                ser.write(packet)
                ser.flush()
                print(f"Sent {len(packet)} bytes:", packet.hex())

                # Step 2: Wait for ACK response
                start_time = time.time()
                timeout_seconds = 5

                while time.time() - start_time < timeout_seconds:
                    if ser.in_waiting >= 3:
                        header = ser.read(3)
                        length = header[1] & 0x0F
                        payload = ser.read(length)
                        full_packet = header + payload

                        if len(full_packet) != 3 + length:
                            print("Incomplete ACK packet received")
                            continue

                        parsed = parse_packet(full_packet)

                        # Step 3: Verify checksum
                        to_checksum = bytes([full_packet[0], full_packet[1]]) + payload
                        computed_crc = crc8(to_checksum)

                        if computed_crc != parsed["checksum"]:
                            # print(f"Checksum mismatch: Received {parsed['checksum']}, Computed {computed_crc}")
                            print("❌ Corrupt reply recieved, retransmit")
                            break

                        # Step 4: Check if ACK
                        if parsed["is_ack"]:
                            print("✅ ACK received")
                            i = i+1
                        else:
                            print("❌ NACK received, retransmit")
                        break

                else:
                    print("Timeout: No packets received in 5 seconds ⏰")
                # time.sleep(2)

    except serial.SerialException as e:
        print(f"Serial error: {e}")
    except KeyboardInterrupt:
        print("Exiting...")



if __name__ == "__main__":
    main()
