import serial
import struct
import duckdb

EVENT_SIZE = 13

def bytes_to_mac(mac_bytes):
    return ":".join(f"{b:02X}" for b in mac_bytes)

# UART setup
ser = serial.Serial('/dev/serial0', 921600, timeout=1)

# DuckDB setup
con = duckdb.connect("events.db")
con.execute("""
CREATE TABLE IF NOT EXISTS events (
    timestamp TIMESTAMP DEFAULT current_timestamp,
    attacker_mac VARCHAR(12),
    sensor_mac VARCHAR(12),
    rssi INTEGER
);
""")

while True:
    if ser.in_waiting >= EVENT_SIZE:
        data = ser.read(EVENT_SIZE)

        attack_mac = bytes_to_mac(data[0:6])
        sensor_mac = bytes_to_mac(data[6:12])
        rssi = struct.unpack("b", data[12:13])[0]

        con.execute(
            "INSERT INTO events (attacker_mac, sensor_mac, rssi) VALUES (?, ?, ?)",
            (attack_mac, sensor_mac, rssi)
        )

        print(f"Inserted: Attacker={attack_mac}, Sensor={sensor_mac}, RSSI={rssi}")
