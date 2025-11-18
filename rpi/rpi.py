import serial
import struct

#typedef struct {
#  uint8_t attack_mac[6]; 6 bytes
#  uint8_t sensor_mac[6]; 6 bytes
#  int8_t attack_rssi; 1 byte
#} wifi_deauth_event_t;
# 6 + 6 + 1 = 13 bytes

EVENT_SIZE = 13

ser = serial.Serial('/dev/serial0', 921600, timeout=1)

while True:
    if ser.in_waiting >= EVENT_SIZE:
        bytes = ser.read(EVENT_SIZE)

        attack_mac = bytes[0:6]
        sensor_mac = bytes[6:12]
        rssi = struct.unpack("b", raw[12:13])[0]

        print("Attack MAC:", ":".join(f"{b:02X}" for b in attack_mac))
        print("Sensor MAC:", ":".join(f"{b:02X}" for b in sensor_mac))
        print("RSSI:", rssi)
        print()
