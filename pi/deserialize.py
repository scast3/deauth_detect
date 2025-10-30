#!/usr/bin/env python3
import serial
import time
import json
from datetime import datetime
import argparse
import sys
from typing import Dict, List, Optional
import statistics

class RSSIDeserializer:
    def __init__(self, port: str = '/dev/ttyUSB0', baudrate: int = 115200):
        """Initialize the RSSI deserializer.
        
        Args:
            port: Serial port (e.g., '/dev/ttyUSB0' for Linux, 'COM3' for Windows)
            baudrate: Baud rate for serial communication
        """
        self.port = port
        self.baudrate = baudrate
        self.serial = None
        self.data_buffer: List[Dict] = []
        self.connect()

    def connect(self) -> None:
        """Establish serial connection with error handling."""
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1  # 1 second timeout
            )
            print(f"Connected to {self.port} at {self.baudrate} baud")
        except serial.SerialException as e:
            print(f"Error opening serial port {self.port}: {e}")
            sys.exit(1)

    def read_packet(self) -> Optional[Dict]:
        """Read a single RSSI packet from the serial port.
        
        Returns:
            Dict containing parsed packet data or None if no valid data
        """
        try:
            if self.serial is None or not self.serial.is_open:
                self.connect()
            
            line = self.serial.readline().decode('utf-8').strip()
            if not line:
                return None

            # Parse the packet - modify this according to your ESP32's data format
            # Example format: {"timestamp": 123456789, "rssi": -70, "source_mac": "00:11:22:33:44:55"}
            try:
                data = json.loads(line)
                # Add reception timestamp
                data['received_at'] = datetime.now().isoformat()
                return data
            except json.JSONDecodeError:
                print(f"Invalid JSON data received: {line}")
                return None

        except serial.SerialException as e:
            print(f"Serial communication error: {e}")
            self.serial = None  # Force reconnection on next attempt
            return None

    def process_packets(self, window_size: int = 100) -> None:
        """Process incoming RSSI packets with basic statistical analysis.
        
        Args:
            window_size: Number of packets to keep for rolling statistics
        """
        try:
            while True:
                packet = self.read_packet()
                if packet:
                    self.data_buffer.append(packet)
                    
                    # Keep only the last window_size packets
                    if len(self.data_buffer) > window_size:
                        self.data_buffer = self.data_buffer[-window_size:]
                    
                    # Calculate rolling statistics
                    rssi_values = [p['rssi'] for p in self.data_buffer]
                    stats = {
                        'current_rssi': packet['rssi'],
                        'avg_rssi': round(statistics.mean(rssi_values), 2),
                        'min_rssi': min(rssi_values),
                        'max_rssi': max(rssi_values),
                        'std_dev': round(statistics.stdev(rssi_values), 2) if len(rssi_values) > 1 else 0
                    }
                    
                    # Print real-time statistics
                    print(f"\rCurrent RSSI: {stats['current_rssi']} dBm | "
                          f"Avg: {stats['avg_rssi']} dBm | "
                          f"Min: {stats['min_rssi']} dBm | "
                          f"Max: {stats['max_rssi']} dBm | "
                          f"Std: {stats['std_dev']} dBm", end='')

                time.sleep(0.01)  # Small delay to prevent CPU overload

        except KeyboardInterrupt:
            print("\nStopping data collection...")
            if self.serial:
                self.serial.close()

def main():
    parser = argparse.ArgumentParser(description='RSSI Data Deserializer')
    parser.add_argument('--port', default='COM3' if sys.platform == 'win32' else '/dev/ttyUSB0',
                      help='Serial port to use')
    parser.add_argument('--baudrate', type=int, default=115200,
                      help='Baud rate for serial communication')
    parser.add_argument('--window', type=int, default=100,
                      help='Window size for rolling statistics')
    
    args = parser.parse_args()
    
    deserializer = RSSIDeserializer(port=args.port, baudrate=args.baudrate)
    deserializer.process_packets(window_size=args.window)

if __name__ == "__main__":
    main()
