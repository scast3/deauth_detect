import serial
import time

# Open UART serial port
ser = serial.Serial(
    port='/dev/serial0',  # or '/dev/ttyS0' or '/dev/ttyAMA0'
    baudrate=115200,
    timeout=1
)

# Give it a moment to initialize
time.sleep(2)

print("Reading UART data...")
try:
    while True:
        if ser.in_waiting > 0:
            data = ser.readline().decode('utf-8', errors='replace').strip()
            print(f"Received: {data}")
except KeyboardInterrupt:
    print("Exiting...")
finally:
    ser.close()
