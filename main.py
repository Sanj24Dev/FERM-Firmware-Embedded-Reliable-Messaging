import serial
import time

SERIAL_PORT = "COM8"
BAUD_RATE = 9600

# Flag masks
PACKET_FLAG_START = 0b10000
PACKET_FLAG_ACK = 0b01000
PACKET_FLAG_FRAGMENT = 0b00100
PACKET_FLAG_FINAL = 0b00010
PACKET_FLAG_RETRANSMIT = 0b00001


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

    # Verify checksum
    computed_crc = crc8(payload)
    if computed_crc != checksum:
        raise ValueError(f"Checksum mismatch! Received: {checksum:#04x}, Computed: {computed_crc:#04x}")


    flags = ( control_byte & 0b11111000 ) >> 3  # upper 5 bits
    ulp   = ( control_byte & 0b00000111 )  # lower 3 bits

    is_sof        = bool(flags & PACKET_FLAG_START)
    is_ack        = bool(flags & PACKET_FLAG_ACK)
    is_fragment   = bool(flags & PACKET_FLAG_FRAGMENT)
    is_final      = bool(flags & PACKET_FLAG_FINAL)
    is_retransmit = bool(flags & PACKET_FLAG_RETRANSMIT)

    data_len = length_byte & 0x0F
    

    print("Parsed Packet:")
    print(f"flags: {flags}")
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
                        parse_packet(packet)
    except serial.SerialException as e:
        print(f"Serial error: {e}")
    except KeyboardInterrupt:
        print("Exiting...")

if __name__ == "__main__":
    main()
