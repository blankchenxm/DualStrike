from serial import Serial
import time
import numpy as np
from collections import deque
from datetime import datetime
import pandas as pd
import keyboard
import atexit
import os


# =============================================================================
# CONFIGURATION PARAMETERS
# =============================================================================

# Serial port configuration
SERIAL_PORT = 'COM33'           # Serial port name
BAUDRATE = 1000000              # Communication baud rate

# Sensor configuration
NUM_SENSORS = 8                 # Number of sensors in the system
PACKET_SIZE_FORMULA = 2 + 4 + (NUM_SENSORS * 12)  # Head  er(2) + Counter(4) + Sensor data(8*12)

# Data recording configuration
OUTPUT_FILENAME = "3.Software/Data/wooting_key_6.csv"  # Output CSV file path
RATE_DISPLAY_INTERVAL = 1.0     # Packet rate display interval (seconds)
TIMESTAMP_BUFFER_SIZE = 1000    # Number of timestamps to keep for rate calculation

# =============================================================================


class SerialReader:
    """
    Serial data reader for 8-sensor magnetometer system.
    
    This class handles reading sensor data from a serial port, calculating packet rates,
    and providing data recording functionality with CSV export.
    """
    
    def __init__(self, port=SERIAL_PORT, baudrate=BAUDRATE):
        """
        Initialize the serial reader with specified port and baud rate.
        
        Args:
            port (str): Serial port name (default: configured SERIAL_PORT)
            baudrate (int): Communication baud rate (default: configured BAUDRATE)
        """
        # Serial connection setup
        self.ser = Serial(port, baudrate)
        
        # Data packet configuration
        self.num_sensors = NUM_SENSORS
        self.packet_size = PACKET_SIZE_FORMULA
        
        # Packet rate calculation
        self.packet_timestamps = deque(maxlen=TIMESTAMP_BUFFER_SIZE)  # Store timestamps of last N packets
        self.last_rate_print = time.time()
        
        # Data recording control
        self.is_recording = False
        self.data_buffer = []  # Buffer to store recorded data
        
        # CSV column names for data export
        self.column_names = ['counter'] + [f'sensor_{i+1}_{axis}' for i in range(NUM_SENSORS) for axis in ['x','y','z']] + ['timestamp']
        
        # Register cleanup function to save data on exit
        atexit.register(self.save_to_csv)
        
    def save_to_csv(self):
        """
        Save recorded data to CSV file.
        
        Creates a DataFrame from the data buffer and exports it to CSV format.
        Automatically creates the output directory if it doesn't exist.
        """
        if self.data_buffer:
            print("Saving data to CSV file...")
            
            # Create DataFrame from buffered data
            df = pd.DataFrame(self.data_buffer, columns=self.column_names)
            filename = OUTPUT_FILENAME
            
            # Ensure output directory exists
            os.makedirs(os.path.dirname(filename), exist_ok=True)
            
            # Export to CSV
            df.to_csv(filename, index=False)
            print(f"Data saved to: {filename}")

    def calculate_packet_rate(self):
        """
        Calculate current packet reception rate.
        
        Returns:
            int: Number of packets received in the last second
        """
        current_time = time.time()
        
        # Remove timestamps older than 1 second
        while self.packet_timestamps and self.packet_timestamps[0] < current_time - 1:
            self.packet_timestamps.popleft()
            
        # Return current packet count (packets per second)
        return len(self.packet_timestamps)
        
    def read_data(self):
        """
        Read and parse one data packet from serial port.
        
        Data packet format:
        - Header: 0xAA 0xBB (2 bytes)
        - Counter: 32-bit little-endian integer (4 bytes)
        - Sensor data: 8 sensors × 3 axes × 4 bytes (float32) = 96 bytes
        
        Returns:
            dict: Parsed data containing counter and sensor readings, or None if error
        """
        try:
            # Wait for packet header (0xAA 0xBB)
            while True:
                if self.ser.read() == b'\xAA' and self.ser.read() == b'\xBB':
                    break
            
            # Record packet reception timestamp for rate calculation
            self.packet_timestamps.append(time.time())
            
            # Read counter value (4 bytes, little-endian)
            counter_bytes = self.ser.read(4)
            counter = int.from_bytes(counter_bytes, 'little')
            
            # Initialize data structure
            data = {
                'counter': counter,
                'sensors': []
            }
            
            # Read data for each sensor (8 sensors total)
            for _ in range(self.num_sensors):
                # Read 3-axis data (each float is 4 bytes)
                x = float(np.frombuffer(self.ser.read(4), dtype=np.float32)[0])
                y = float(np.frombuffer(self.ser.read(4), dtype=np.float32)[0])
                z = float(np.frombuffer(self.ser.read(4), dtype=np.float32)[0])
                
                data['sensors'].append({
                    'x': x,
                    'y': y,
                    'z': z
                })
                
            # If recording is active, add data to buffer
            if self.is_recording:
                # Flatten data structure for CSV export
                flat_data = [data['counter']]
                for sensor in data['sensors']:
                    flat_data.extend([sensor['x'], sensor['y'], sensor['z']])
                flat_data.append(time.time())  # Add timestamp
                self.data_buffer.append(flat_data)
                
            return data
            
        except Exception as e:
            print(f"Read error: {e}")
            return None
            
    def close(self):
        """
        Close the serial connection.
        """
        self.ser.close()


def main():
    """
    Main function to run the serial data reader.
    
    Sets up the serial reader, handles keyboard input for recording control,
    and displays real-time packet rate information.
    """
    # Create serial reader instance with configured parameters
    reader = SerialReader()
    
    def handle_space():
        """
        Toggle data recording on/off when space key is pressed.
        """
        reader.is_recording = not reader.is_recording
        status = "Started" if reader.is_recording else "Stopped"
        print(f"\n{status} recording data...")
    
    # Register space key event handler
    keyboard.on_press_key('space', lambda _: handle_space())
    
    try:
        print(f"Serial data reader started on {SERIAL_PORT} at {BAUDRATE} baud.")
        print(f"Output file: {OUTPUT_FILENAME}")
        print("Press SPACE to toggle recording, Ctrl+C to exit.")
        
        while True:
            # Read data packet
            data = reader.read_data()
            
            if data:
                current_time = time.time()
                
                # Update packet rate display every configured interval
                if current_time - reader.last_rate_print >= RATE_DISPLAY_INTERVAL:
                    packet_rate = reader.calculate_packet_rate()
                    print(f"\nCurrent packet rate: {packet_rate} packets/sec")
                    reader.last_rate_print = current_time
                
                # # Uncomment the following lines to display sensor data in real-time
                # print(f"Counter: {data['counter']}")
                # for i, sensor in enumerate(data['sensors']):
                #     print(f"Sensor {i+1}: X={sensor['x']:.2f}, Y={sensor['y']:.2f}, Z={sensor['z']:.2f}")
                # print("-" * 50)
                
                # Display recording status
                if reader.is_recording:
                    print("Recording data...")
                
    except KeyboardInterrupt:
        print("\nProgram stopped")
        reader.close()


if __name__ == "__main__":
    main()