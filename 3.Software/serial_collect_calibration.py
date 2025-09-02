from serial import Serial
import time
import numpy as np
from collections import deque
from datetime import datetime
import pandas as pd
import keyboard
import atexit


# =============================================================================
# CONFIGURATION PARAMETERS
# =============================================================================

# Serial port configuration
SERIAL_PORT = 'COM33'           # Serial port name
BAUDRATE = 1000000              # Communication baud rate

# Sensor configuration
NUM_SENSORS = 8                 # Number of sensors in the system
PACKET_SIZE_FORMULA = 2 + 4 + (NUM_SENSORS * 12)  # Header(2) + Counter(4) + Sensor data(8*12)

# Data recording configuration
OUTPUT_FILENAME = r"3.Software\Data\calibration\calibration3.csv"  # Output CSV file path
RATE_DISPLAY_INTERVAL = 1.0     # Packet rate display interval (seconds)
TIMESTAMP_BUFFER_SIZE = 1000    # Number of timestamps to keep for rate calculation

# Key detection configuration
KEY_EVENT_WINDOW = 0.5          # Time window for key detection (seconds before/after)

CALIBRATION_SEQUENCE = "qruzvm"  # Calibration sequence qruzvvmto detect

# Final key recording configuration
FINAL_KEY_WAIT_TIME = 0.1    # Additional wait time after final key recording (seconds)

# =============================================================================


class SerialReader:
    """
    Serial data reader for 8-sensor magnetometer system with key press detection.
    
    This class handles reading sensor data from a serial port, detecting key presses,
    and providing data recording functionality with CSV export including keystroke information.
    """
    
    def __init__(self, port=SERIAL_PORT, baudrate=BAUDRATE):
        """
        Initialize the serial reader with specified port and baud rate.
        
        Args:
            port (str): Serial port name (default: configured SERIAL_PORT)
            baudrate (int): Communication baud rate (default: configured BAUDRATE)
        """
        # Serial connection setup
        try:
            self.ser = Serial(port, baudrate, timeout=1)  # Add 1 second timeout
            print(f"Successfully connected to {port} at {baudrate} baud")
        except Exception as e:
            print(f"Failed to connect to serial port {port}: {e}")
            raise
        
        # Data packet configuration
        self.num_sensors = NUM_SENSORS
        self.packet_size = PACKET_SIZE_FORMULA
        
        # Packet rate calculation
        self.packet_timestamps = deque(maxlen=TIMESTAMP_BUFFER_SIZE)  # Store timestamps of last N packets
        self.last_rate_print = time.time()
        
        # Data recording
        self.data_buffer = []  # Buffer to store all recorded data
        
        # CSV column names for data export
        self.column_names = ['counter'] + [f'sensor_{i+1}_{axis}' for i in range(NUM_SENSORS) for axis in ['x','y','z']] + ['timestamp', 'key_press']
        
        # Key event detection queue
        self.key_events = deque()
        
        # Calibration sequence tracking
        self.calibration_sequence = CALIBRATION_SEQUENCE
        self.current_sequence_index = 0
        self.calibration_complete = False
        
        # Program running flag
        self.is_running = True
        
        # Final key recording tracking
        self.final_key_detected = False
        self.final_key_time = None
        self.final_recording_complete = False
        
        # Register cleanup function to save data on exit
        atexit.register(self.save_to_csv)
        
    def save_to_csv(self):
        """
        Save recorded data to CSV file.
        
        Creates a DataFrame from the data buffer and exports it to CSV format.
        Includes both sensor data and key press information.
        """
        print(f"Attempting to save data... Buffer contains {len(self.data_buffer)} records")
        
        if self.data_buffer:
            print("Saving data to CSV file...")
            
            # Create DataFrame from buffered data
            df = pd.DataFrame(self.data_buffer, columns=self.column_names)
            filename = OUTPUT_FILENAME
            
            # Export to CSV
            df.to_csv(filename, index=False)
            print(f"Data saved to: {filename}")
        else:
            print("Warning: No data to save! Data buffer is empty.")
            print("This might be because no serial data was received during the recording period.")

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
        
    def check_calibration_sequence(self, key_name):
        """
        Check if the pressed key matches the expected calibration sequence.
        
        Args:
            key_name (str): Name of the pressed key
            
        Returns:
            bool: True if calibration is complete, False otherwise
        """
        # Check if calibration is already complete
        if self.current_sequence_index >= len(self.calibration_sequence):
            return True
            
        expected_key = self.calibration_sequence[self.current_sequence_index]
        
        if key_name == expected_key:
            self.current_sequence_index += 1
            print(f"Calibration sequence progress: {self.current_sequence_index}/{len(self.calibration_sequence)}")
            
            if self.current_sequence_index == len(self.calibration_sequence):
                self.calibration_complete = True
                self.final_key_detected = True
                self.final_key_time = time.time()
                print("Calibration recording完成！正在记录最后一个按键的前后数据...")
                return True
        else:
            print(f"错误：期待按键 '{expected_key}' 但收到 '{key_name}'")
            print("校准序列错误，程序终止")
            self.is_running = False
            return False
        
        return False
        
    def check_final_recording_complete(self):
        """
        Check if final key recording is complete.
        
        Returns:
            bool: True if final recording is complete and we should stop
        """
        if not self.final_key_detected:
            return False
            
        current_time = time.time()
        time_since_final_key = current_time - self.final_key_time
        
        # Check if we have recorded enough data after the final key
        if time_since_final_key >= KEY_EVENT_WINDOW + FINAL_KEY_WAIT_TIME:
            if not self.final_recording_complete:
                print("最后按键数据记录完成，程序即将结束...")
                self.final_recording_complete = True
            return True
            
        return False
        
    def stop(self):
        """
        Stop the data reader and mark for exit.
        """
        self.is_running = False
        
    def read_data(self):
        """
        Read and parse one data packet from serial port with key detection.
        
        Data packet format:
        - Header: 0xAA 0xBB (2 bytes)
        - Counter: 32-bit little-endian integer (4 bytes)
        - Sensor data: 8 sensors × 3 axes × 4 bytes (float32) = 96 bytes
        
        Returns:
            dict: Parsed data containing counter and sensor readings, or None if error
        """
        try:
            # Check if we should still be running
            if not self.is_running:
                return None
                
            # Debug: Check if serial port is open
            if not self.ser.is_open:
                print("Warning: Serial port is not open!")
                return None
                
            # Wait for packet header (0xAA 0xBB)
            while True:
                if not self.is_running:
                    return None
                byte1 = self.ser.read(1)
                if not byte1:  # Timeout occurred
                    return None
                byte2 = self.ser.read(1)
                if not byte2:  # Timeout occurred
                    return None
                if byte1 == b'\xAA' and byte2 == b'\xBB':
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
                
            # Record data with key detection
            # Flatten data structure for CSV export
            flat_data = [data['counter']]
            for sensor in data['sensors']:
                flat_data.extend([sensor['x'], sensor['y'], sensor['z']])
            current_time = time.time()
            flat_data.append(current_time)  # Add timestamp
            
            # Check if current time is within any key event time window
            key_type = 'none'  # Default no key press
            for key_event in self.key_events:
                # Use different window for final key detection
                if self.final_key_detected and key_event == self.key_events[-1]:
                    # For the final key, use the longer recording window
                    if current_time >= self.final_key_time - KEY_EVENT_WINDOW and current_time <= self.final_key_time + KEY_EVENT_WINDOW:
                        key_type = key_event[0]
                        break
                else:
                    # For regular keys, use normal window
                    if current_time >= key_event[1] - KEY_EVENT_WINDOW and current_time <= key_event[1] + KEY_EVENT_WINDOW:
                        key_type = key_event[0]
                        break
            
            flat_data.append(key_type)  # Add key press type
            self.data_buffer.append(flat_data)
            
            # Clean up expired key events (except final key)
            while self.key_events and current_time > self.key_events[0][1] + KEY_EVENT_WINDOW:
                if self.final_key_detected and self.key_events[0][1] == self.final_key_time:
                    break  # Don't remove final key event yet
                self.key_events.popleft()
                
            return data
            
        except Exception as e:
            if self.is_running:
                print(f"Read error: {e}")
            return None
            
    def close(self):
        """
        Close the serial connection.
        """
        if self.ser.is_open:
            self.ser.close()


def main():
    """
    Main function to run the serial data reader with calibration sequence detection.
    
    Sets up the serial reader, handles keyboard input for calibration sequence detection,
    and displays real-time packet rate information.
    """
    # Create serial reader instance with configured parameters
    reader = SerialReader(port=SERIAL_PORT)
    
    def on_key_press(event):
        """
        Handle key press events for calibration sequence detection.
        
        Args:
            event: Keyboard event object containing key information
        """
        # Ignore Ctrl+C combination to allow clean exit
        if event.name == 'c' and keyboard.is_pressed('ctrl'):
            return
            
        # Add key event to detection queue
        reader.key_events.append((event.name, time.time()))
        print(f"\nKey detected: {event.name}")
        
        # Check calibration sequence
        reader.check_calibration_sequence(event.name)
    
    # Register key press event handler
    keyboard.on_press(on_key_press)
    
    try:
        print(f"Serial data reader started on {SERIAL_PORT} at {BAUDRATE} baud.")
        print(f"Output file: {OUTPUT_FILENAME}")
        print(f"Calibration sequence: {CALIBRATION_SEQUENCE}")
        print(f"Please input the calibration sequence: {CALIBRATION_SEQUENCE}")
        print("按 Ctrl+C 退出程序")
        
        while reader.is_running:
            # Read data packet
            data = reader.read_data()
            
            # Always check if final recording is complete, regardless of data
            if reader.check_final_recording_complete():
                reader.stop()
                break
            
            if data and reader.is_running:
                current_time = time.time()
                
                # Update packet rate display every configured interval
                if current_time - reader.last_rate_print >= RATE_DISPLAY_INTERVAL:
                    packet_rate = reader.calculate_packet_rate()
                    print(f"\nCurrent packet rate: {packet_rate} packets/sec")
                    reader.last_rate_print = current_time
                    
        # 程序正常结束
        print("程序正常结束")
        reader.save_to_csv()
        
    except KeyboardInterrupt:
        print("\nProgram stopped by user")
        reader.stop()
        
    finally:
        reader.close()


if __name__ == "__main__":
    main()