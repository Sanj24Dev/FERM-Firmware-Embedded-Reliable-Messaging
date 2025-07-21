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
    

    print("Parsed Packet:")
    print(f"  is_sof       : {is_sof}")
    print(f"  is_ack       : {is_ack}")
    print(f"  is_fragment  : {is_fragment}")
    print(f"  is_final     : {is_final}")
    print(f"  is_retransmit: {is_retransmit}")
    print(f"  ULP field    : {ulp}")
    print(f"  Data Length  : {data_len}")
    print(f"  Checksum     : {checksum}")
    print("  Payload      :", [int(b) for b in payload])

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

    # Control byte: flags (5 bits) + ULP (3 bits)
    control_byte = ((flags & 0x1F) << 3) | (ulp & 0x07)

    # Length byte: payload length (4 bits) + reserved (4 bits set to 0)
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
    try:
        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
            print(f"Listening on {SERIAL_PORT} at {BAUD_RATE} baud...")
            time.sleep(2)

            while True:
                if ser.in_waiting >= 3:
                    header = ser.read(3)
                    length = header[1] & 0x0F
                    payload = ser.read(length)
                    packet = header + payload
                    if len(packet) == 3 + length:
                        recvd_packet = parse_packet(packet)
                        # Verify checksum
                        to_checksum = bytes([packet[0], packet[1]]) + payload
                        computed_crc = crc8(to_checksum)
                        recvd_checksum = recvd_packet["checksum"]
                        if computed_crc != recvd_checksum:
                            print(f"Checksum mismatch! Received: {recvd_checksum}, Computed: {computed_crc}")

                        else:
                            print("Valid data received\n")
                            data = bytes([random.randint(0, 255)])
                            flags = PACKET_FLAG_ACK
                            ulp = ULP_UART
                            packet = build_ferm_packet(flags, ulp, data)

                            # Send the packet
                            try:
                                ser.write(packet)
                                print(f"Sent {len(packet)} bytes:", packet.hex())
                            except serial.SerialException as e:
                                print("Serial error:", e)



    except serial.SerialException as e:
        print(f"Serial error: {e}")
    except KeyboardInterrupt:
        print("Exiting...")

if __name__ == "__main__":
    main()
