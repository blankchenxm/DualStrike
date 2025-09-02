from serial import Serial
import time
import numpy as np
from collections import deque
from datetime import datetime
import pandas as pd
import keyboard
import atexit

import torch
import torch.nn as nn
import numpy as np
from classify import KeypressClassifier

class KeypressPredictor:
    """
    Real-time keypress prediction model using magnetic field data from 8 sensors.
    
    This class loads a pre-trained neural network model and provides functionality
    to predict keypress types based on magnetic field peak data from multiple sensors.
    """
    
    def __init__(self, model_path='keypress_model.pth'):
        """
        Initialize the keypress predictor with a pre-trained model.
        
        Args:
            model_path (str): Path to the saved model file containing weights and label encoder
        """
        # Load model and label encoder
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        checkpoint = torch.load(model_path, weights_only=False)
        
        # Initialize model architecture
        self.model = KeypressClassifier(checkpoint['num_classes']).to(self.device)
        self.model.load_state_dict(checkpoint['model_state_dict'])
        self.model.eval()
        
        # Load label encoder for converting predictions back to key names
        self.label_encoder = checkpoint['label_encoder']
    
    def predict(self, peak_data):
        """
        Predict keypress type from magnetic field peak data.
        
        Args:
            peak_data (numpy.ndarray): Shape (8, 3) array containing 3-axis data from 8 sensors
        
        Returns:
            tuple: (predicted_label, probability)
                - predicted_label (str): Predicted key type
                - probability (float): Prediction confidence probability
        
        Raises:
            ValueError: If input data shape is not (8, 3)
        """
        # Ensure input data format is correct
        if peak_data.shape != (8, 3):
            raise ValueError("Input data shape must be (8, 3)")
        
        # Reshape data to model's expected format (1, 1, 8, 3)
        data = torch.FloatTensor(peak_data.reshape(1, 1, 8, 3)).to(self.device)
        
        # Perform prediction
        with torch.no_grad():
            outputs = self.model(data)
            probabilities = torch.nn.functional.softmax(outputs, dim=1)
            
            # Get the highest probability prediction
            prob, predicted = torch.max(probabilities, 1)
            predicted_label = self.label_encoder.inverse_transform([predicted.item()])[0]
            
            return predicted_label, prob.item()

class SerialReader:
    """
    Real-time serial data reader and processor for magnetic field sensors.
    
    This class handles:
    - Serial communication with magnetic field sensor array
    - Real-time data filtering and calibration
    - Peak detection for keypress events
    - Automatic data logging and prediction
    """
    
    # ==================== CONFIGURATION PARAMETERS ====================
    
    # Serial Communication Parameters
    DEFAULT_PORT = 'COM33'                    # Default serial port
    DEFAULT_BAUDRATE = 1000000               # Serial communication baud rate
    NUM_SENSORS = 8                          # Number of magnetic field sensors
    PACKET_HEADER_SIZE = 2                   # Header bytes (0xAA, 0xBB)
    COUNTER_SIZE = 4                         # Counter field size in bytes
    SENSOR_DATA_SIZE = 12                    # Each sensor: 3 axes × 4 bytes per float
    
    # Moving Average Filter Parameters
    FILTER_WINDOW_SIZE = 15                  # Moving average filter window size
    
    # Calibration Parameters
    CALIBRATION_WINDOW_SIZE = 50             # Window size for finding stable baseline
    CALIBRATION_STD_THRESHOLD = 3            # Standard deviation threshold for stability
    CALIBRATION_BUFFER_MIN_SIZE = 20         # Minimum buffer size for base level calculation
    
    # Envelope Calculation Parameters
    SAMPLING_RATE = 250                      # Data sampling frequency (Hz)
    ENVELOPE_WINDOW_SECONDS = 0.05           # Time window for envelope calculation (seconds)
    
    # Peak Detection Parameters
    PEAK_SLOPE_THRESHOLD = 0.16              # Minimum slope for rising/falling edge detection
    PEAK_AMPLITUDE_THRESHOLD = 5             # Minimum amplitude above baseline for peak
    PEAK_MIN_DURATION_COUNT = 3              # Minimum consecutive samples for valid peak (~12ms)
    PEAK_TIMEOUT_SECONDS = 0.012             # Maximum time before peak auto-ends
    PEAK_VALUES_BUFFER_SIZE = 50             # Buffer size for base level calculation
    PEAK_BASE_PERCENTILE = 10                # Percentile for baseline calculation
    
    # Data Recording Parameters
    RECORDING_TOGGLE_KEY = 'space'           # Key to toggle data recording
    
    def __init__(self, port=DEFAULT_PORT, baudrate=DEFAULT_BAUDRATE):
        """
        Initialize the serial reader with all necessary buffers and parameters.
        
        Args:
            port (str): Serial port name (e.g., 'COM3', '/dev/ttyUSB0')
            baudrate (int): Serial communication baud rate
        """
        # Initialize serial connection
        self.ser = Serial(port, baudrate)
        
        # Calculate derived parameters
        self.packet_size = (self.PACKET_HEADER_SIZE + self.COUNTER_SIZE + 
                           (self.NUM_SENSORS * self.SENSOR_DATA_SIZE))
        self.envelope_window_points = int(self.ENVELOPE_WINDOW_SECONDS * self.SAMPLING_RATE)
        self.envelope_half_window = self.envelope_window_points // 2
        
        # Initialize moving average filter buffers
        # Each sensor has separate buffers for X, Y, Z axes
        self.data_buffer = {
            f'sensor_{i+1}': {
                'x': deque(maxlen=self.FILTER_WINDOW_SIZE),
                'y': deque(maxlen=self.FILTER_WINDOW_SIZE),
                'z': deque(maxlen=self.FILTER_WINDOW_SIZE)
            } for i in range(self.NUM_SENSORS)
        }
        
        # Initialize calibration buffers for offset calculation
        # Used to find stable baseline during initial calibration phase
        self.calibration_buffer = {
            f'sensor_{i+1}': {
                'x': deque(maxlen=self.CALIBRATION_WINDOW_SIZE),
                'y': deque(maxlen=self.CALIBRATION_WINDOW_SIZE),
                'z': deque(maxlen=self.CALIBRATION_WINDOW_SIZE)
            } for i in range(self.NUM_SENSORS)
        }
        
        # Calibration state variables
        self.offsets = None                   # Calculated offset values for each sensor
        self.is_calibrated = False            # Whether initial calibration is complete
        
        # Initialize envelope calculation buffers
        # Used for peak detection by finding maximum absolute values in time windows
        self.envelope_buffers = {
            f'sensor_{i+1}': {
                'x': deque(maxlen=self.envelope_window_points),
                'y': deque(maxlen=self.envelope_window_points),
                'z': deque(maxlen=self.envelope_window_points)
            } for i in range(self.NUM_SENSORS)
        }
        
        # Initialize peak detection state for each sensor
        # Tracks current peak status and characteristics
        self.peak_status = {
            f'sensor_{i+1}': {
                'in_peak': False,                                           # Currently detecting a peak
                'peak_start': 0,                                           # Peak start timestamp
                'rising_count': 0,                                         # Consecutive rising samples
                'falling_count': 0,                                        # Consecutive falling samples
                'base_level': 0,                                           # Dynamic baseline level
                'values_buffer': deque(maxlen=self.PEAK_VALUES_BUFFER_SIZE) # Buffer for baseline calculation
            } for i in range(self.NUM_SENSORS)
        }
        
        # Peak maximum value tracking for prediction
        # Stores the sensor data at the moment of peak maximum for keypress prediction
        self.peak_max_data = {
            f'sensor_{i+1}': {
                'max_value': 0,                                            # Peak maximum amplitude
                'max_data': None                                           # Complete sensor data at peak maximum
            } for i in range(self.NUM_SENSORS)
        }
        
        # Data recording system
        self.is_recording = False             # Recording state toggle
        self.recorded_data = []               # List to store recorded data points
        
        # Define CSV column names for data export
        self.column_names = ['counter'] + [f'sensor_{i+1}_{axis}_{data_type}' 
                           for i in range(8) 
                           for axis in ['x','y','z'] 
                           for data_type in ['raw', 'filtered', 'calibrated', 'envelope']] + ['timestamp']
        
        # Register cleanup function for graceful shutdown
        atexit.register(self.save_to_csv)
        
        # Initialize keypress prediction model
        try:
            self.predictor = KeypressPredictor('3.Software\wooting_keypress_model2.pth')
            print("Successfully loaded keypress prediction model")
        except Exception as e:
            print(f"Failed to load prediction model: {e}")
            self.predictor = None

    def save_to_csv(self):
        """
        Save all recorded data to a CSV file with timestamp in filename.
        Called automatically on program exit if data was recorded.
        """
        if self.recorded_data:
            print("Saving data to CSV file...")
            df = pd.DataFrame(self.recorded_data, columns=self.column_names)
            filename = f"sensor_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
            df.to_csv(filename, index=False)
            print(f"Data saved to: {filename}")

    def apply_moving_average_filter(self, sensor_id, axis):
        """
        Apply moving average filter to smooth sensor data and reduce noise.
        
        Args:
            sensor_id (int): Sensor ID (1-8)
            axis (str): Axis name ('x', 'y', or 'z')
        
        Returns:
            float or None: Filtered value, or None if insufficient data
        """
        data_queue = self.data_buffer[f'sensor_{sensor_id}'][axis]
        if len(data_queue) >= self.FILTER_WINDOW_SIZE:
            data = np.array(data_queue)
            filtered = np.mean(data)
            return filtered
        return None

    def check_calibration(self, sensor_id):
        """
        Check if sensor data is stable enough for calibration offset calculation.
        
        Args:
            sensor_id (int): Sensor ID (1-8)
        
        Returns:
            bool: True if all axes are stable (low standard deviation), False otherwise
        """
        for axis in ['x', 'y', 'z']:
            data = np.array(self.calibration_buffer[f'sensor_{sensor_id}'][axis])
            if len(data) < self.CALIBRATION_WINDOW_SIZE:
                return False
            if np.std(data) > self.CALIBRATION_STD_THRESHOLD:
                return False
        return True

    def calculate_offsets(self, sensor_id):
        """
        Calculate sensor offset values from stable calibration data.
        
        Args:
            sensor_id (int): Sensor ID (1-8)
        
        Returns:
            dict: Offset values for each axis {'x': offset_x, 'y': offset_y, 'z': offset_z}
        """
        offsets = {}
        for axis in ['x', 'y', 'z']:
            data = np.array(self.calibration_buffer[f'sensor_{sensor_id}'][axis])
            offsets[axis] = np.mean(data)
        return offsets

    def calculate_envelope(self, sensor_id, axis, value):
        """
        Calculate envelope value for peak detection using sliding window maximum.
        
        The envelope is calculated by finding the maximum absolute value within
        a time window around each data point. This helps in detecting the peak
        characteristics of magnetic field changes during keypresses.
        
        Args:
            sensor_id (int): Sensor ID (1-8)
            axis (str): Axis name ('x', 'y', or 'z')
            value (float): Current calibrated sensor value
        
        Returns:
            float: Envelope value (maximum absolute value in window, or current value if window not full)
        """
        buffer = self.envelope_buffers[f'sensor_{sensor_id}'][axis]
        buffer.append(value)
        
        if len(buffer) >= self.envelope_window_points:
            # Get center point index for the sliding window
            center_idx = len(buffer) - self.envelope_half_window - 1
            if center_idx >= 0:
                # Find maximum absolute value across the entire window
                window_data = list(buffer)
                max_abs_idx = np.argmax(np.abs(window_data))
                return window_data[max_abs_idx]
        
        return value

    def detect_peak_realtime(self, sensor_id, total_field, processed_data):
        """
        Real-time peak detection algorithm for keypress event identification.
        
        This method implements a state machine that:
        1. Monitors for rising edges above threshold (start of keypress)
        2. Tracks peak maximum during keypress event
        3. Detects falling edges and end of keypress
        4. Triggers prediction when peak is complete
        
        Args:
            sensor_id (int): Sensor ID (1-8)
            total_field (float): Current total magnetic field magnitude
            processed_data (dict): Complete processed sensor data for all sensors
        """
        status = self.peak_status[f'sensor_{sensor_id}']
        status['values_buffer'].append(total_field)
        
        # Dynamically update baseline level using percentile of recent values
        if len(status['values_buffer']) >= self.CALIBRATION_BUFFER_MIN_SIZE:
            status['base_level'] = np.percentile(list(status['values_buffer']), self.PEAK_BASE_PERCENTILE)
        
        # Calculate current slope using simple difference
        values = list(status['values_buffer'])
        if len(values) >= 2:
            current_slope = values[-1] - values[-2]
        else:
            return
        
        # Determine current signal characteristics
        is_rising = current_slope > self.PEAK_SLOPE_THRESHOLD
        is_falling = current_slope < -self.PEAK_SLOPE_THRESHOLD
        is_above_threshold = total_field > status['base_level'] + self.PEAK_AMPLITUDE_THRESHOLD
        
        # State machine for peak detection
        if not status['in_peak']:
            # Look for peak start: sustained rising slope above threshold
            if is_rising and is_above_threshold:
                status['rising_count'] += 1
                if status['rising_count'] >= self.PEAK_MIN_DURATION_COUNT:
                    status['in_peak'] = True
                    status['peak_start'] = time.time()
                    status['rising_count'] = 0
                    print(f"Sensor {sensor_id} peak start detected!")
                    # Reset maximum value tracking
                    self.peak_max_data[f'sensor_{sensor_id}']['max_value'] = 0
                    self.peak_max_data[f'sensor_{sensor_id}']['max_data'] = None
        else:
            # During peak: track maximum value and watch for peak end
            if total_field > self.peak_max_data[f'sensor_{sensor_id}']['max_value']:
                self.peak_max_data[f'sensor_{sensor_id}']['max_value'] = total_field
                self.peak_max_data[f'sensor_{sensor_id}']['max_data'] = processed_data
            
            # Look for peak end: sustained falling or return to baseline
            if is_falling:
                status['falling_count'] += 1
                if status['falling_count'] >= self.PEAK_MIN_DURATION_COUNT and not is_above_threshold:
                    print(f"Sensor {sensor_id} peak end detected, duration: {time.time() - status['peak_start']:.3f}s")
                    self._print_peak_max_data(sensor_id)
                    status['in_peak'] = False
                    status['falling_count'] = 0
            elif not is_above_threshold:
                # Peak ended by returning to baseline level
                if time.time() - status['peak_start'] > self.PEAK_TIMEOUT_SECONDS:
                    print(f"Sensor {sensor_id} peak end (baseline return), duration: {time.time() - status['peak_start']:.3f}s")
                    self._print_peak_max_data(sensor_id)
                status['in_peak'] = False
                status['falling_count'] = 0

    def _print_peak_max_data(self, sensor_id):
        """
        Print peak maximum data and perform keypress prediction.
        
        This method is called when a peak is detected and completed. It:
        1. Extracts envelope data from all 8 sensors at peak maximum
        2. Formats data for neural network prediction
        3. Calls the prediction model if available
        4. Displays results in human-readable format
        
        Args:
            sensor_id (int): Sensor ID that triggered the peak detection (1-8)
        """
        max_data = self.peak_max_data[f'sensor_{sensor_id}']['max_data']
        if max_data:
            print(f"\nKeypress detected - Sensor {sensor_id} peak maximum 8x3 data:")
            
            # Collect envelope data from all sensors for prediction
            peak_data = np.zeros((8, 3))
            for i, sensor_data in enumerate(max_data['sensors']):
                # Store envelope data for neural network input
                peak_data[i] = [
                    sensor_data['envelope']['x'],
                    sensor_data['envelope']['y'],
                    sensor_data['envelope']['z']
                ]
            
            # Display 8x3 peak data matrix
            print("8x3 Peak Data (X, Y, Z):")
            for i in range(8):
                print(f"Sensor{i+1}: [{peak_data[i][0]:6.2f}, {peak_data[i][1]:6.2f}, {peak_data[i][2]:6.2f}]")
            
            # Perform keypress prediction if model is available
            if self.predictor is not None:
                try:
                    predicted_label, probability = self.predictor.predict(peak_data)
                    print(f"\nPredicted key type: {predicted_label}")
                    print(f"Prediction probability: {probability:.2f}")
                except Exception as e:
                    print(f"Prediction error: {e}")

    def read_data(self):
        """
        Read and process a complete data packet from the sensor array.
        
        This method handles the complete data processing pipeline:
        1. Read raw sensor data from serial port
        2. Apply moving average filtering
        3. Perform calibration offset correction
        4. Calculate envelope for peak detection
        5. Trigger peak detection and prediction
        6. Log data if recording is enabled
        
        Returns:
            dict or None: Processed data containing all sensor information, or None if error occurred
        """
        try:
            # Batch read data packets from serial port
            header = self.ser.read_until(b'\xAA\xBB')
            payload = self.ser.read(self.packet_size - 2)
            
            if len(payload) != self.packet_size - 2:
                print(f"Incorrect data packet length: Expected {self.packet_size - 2}, got {len(payload)}")
                return None
            
            # Parse counter field from packet
            counter = int.from_bytes(payload[0:4], 'little')
            
            # Initialize data structure for processed sensor data
            processed_data = {
                'counter': counter,
                'timestamp': time.time(),
                'sensors': []
            }
            
            # Read and process each sensor's data
            data_offset = 4  # Skip 4 bytes for counter
            for sensor_id in range(1, self.NUM_SENSORS + 1):
                sensor_data = {'raw': {}, 'filtered': {}, 'calibrated': {}, 'envelope': {}}
                
                # Read raw data
                for axis in ['x', 'y', 'z']:
                    value = float(np.frombuffer(payload[data_offset:data_offset+4], dtype=np.float32)[0])
                    data_offset += 4
                    sensor_data['raw'][axis] = value
                    
                    # Debug: Check if raw data is normal
                    if abs(value) > 1e6:  # Check for unusually large values
                        print(f"WARNING: Sensor {sensor_id} {axis} axis raw data abnormal: {value}")
                    
                    # Update filter buffer
                    self.data_buffer[f'sensor_{sensor_id}'][axis].append(value)
                
                # Apply filter
                for axis in ['x', 'y', 'z']:
                    filtered_value = self.apply_moving_average_filter(sensor_id, axis)
                    sensor_data['filtered'][axis] = filtered_value if filtered_value is not None else sensor_data['raw'][axis]
                    
                    # Debug: Check if filtered data is normal
                    if filtered_value is not None and abs(filtered_value) > 1e6:
                        print(f"WARNING: Sensor {sensor_id} {axis} axis filtered data abnormal: {filtered_value}")
                    
                    # Update calibration buffer
                    if not self.is_calibrated:
                        self.calibration_buffer[f'sensor_{sensor_id}'][axis].append(filtered_value if filtered_value is not None else sensor_data['raw'][axis])
                
                # Check if calibration is needed
                if not self.is_calibrated:
                    if self.check_calibration(sensor_id):
                        if self.offsets is None:
                            self.offsets = {}
                        self.offsets[f'sensor_{sensor_id}'] = self.calculate_offsets(sensor_id)
                        if len(self.offsets) == self.NUM_SENSORS:
                            self.is_calibrated = True
                            print("All sensors calibrated!")
                            print("Offset values:")
                            for sensor_key, offset_values in self.offsets.items():
                                print(f"{sensor_key}: X={offset_values['x']:.2f}, Y={offset_values['y']:.2f}, Z={offset_values['z']:.2f}")
                
                # Apply offset correction and calculate envelope
                if self.is_calibrated:
                    for axis in ['x', 'y', 'z']:
                        calibrated_value = sensor_data['filtered'][axis] - self.offsets[f'sensor_{sensor_id}'][axis]
                        sensor_data['calibrated'][axis] = calibrated_value
                        
                        # Debug: Check if calibrated data is normal
                        if abs(calibrated_value) > 1e6:
                            print(f"WARNING: Sensor {sensor_id} {axis} axis calibrated data abnormal: {calibrated_value}")
                            print(f"  Filtered value: {sensor_data['filtered'][axis]}")
                            print(f"  Offset value: {self.offsets[f'sensor_{sensor_id}'][axis]}")
                        
                        # Calculate envelope for each axis separately
                        envelope_value = self.calculate_envelope(sensor_id, axis, calibrated_value)
                        sensor_data['envelope'][axis] = envelope_value
                        
                        # Debug: Check if envelope data is normal
                        if abs(envelope_value) > 1e6:
                            print(f"WARNING: Sensor {sensor_id} {axis} axis envelope data abnormal: {envelope_value}")
                
                processed_data['sensors'].append(sensor_data)
                
                # If recording, save data
                if self.is_recording:
                    flat_data = [counter]
                    for sensor in processed_data['sensors']:
                        for data_type in ['raw', 'filtered', 'calibrated', 'envelope']:
                            for axis in ['x', 'y', 'z']:
                                flat_data.append(sensor[data_type].get(axis, 0))
                    flat_data.append(processed_data['timestamp'])
                    self.recorded_data.append(flat_data)
            
            # Add peak detection after envelope calculation
            if self.is_calibrated:
                for i, sensor_data in enumerate(processed_data['sensors'], 1):
                    if 'envelope' in sensor_data:
                        envelope_total = np.sqrt(
                            sensor_data['envelope']['x']**2 + 
                            sensor_data['envelope']['y']**2 + 
                            sensor_data['envelope']['z']**2
                        )
                        self.detect_peak_realtime(i, envelope_total, processed_data)
            
            return processed_data
            
        except Exception as e:
            print(f"Read error: {e}")
            return None
            
    def close(self):
        """
        Close the serial connection gracefully.
        """
        self.ser.close()

def main():
    reader = SerialReader(port='COM33')  # Modify port number as needed
    
    def handle_space():
        reader.is_recording = not reader.is_recording
        status = "Start" if reader.is_recording else "Stop"
        print(f"\n{status} recording data...")
    
    keyboard.on_press_key('space', lambda _: handle_space())
    
    print("Waiting for sensor calibration...")
    try:
        while True:
            data = reader.read_data()
            if data:
                if not reader.is_calibrated:
                    print("Calibrating...", end='\r')
                    # Temporarily: Display raw data for debugging
                    if data['counter'] % 50 == 0:  # Display every 50 data packets
                        print(f"\nDebug - Counter: {data['counter']}")
                        for i, sensor_data in enumerate(data['sensors'], 1):
                            if 'raw' in sensor_data:
                                print(f"Sensor {i}: X={sensor_data['raw']['x']:.2f}, Y={sensor_data['raw']['y']:.2f}, Z={sensor_data['raw']['z']:.2f}")
                        print("-" * 50)
                else:
                    # Display processed data
                    for i, sensor_data in enumerate(data['sensors'], 1):
                        # Detailed data structure debugging
                        # print(f"\nSensor {i} data structure:")
                        # for key in sensor_data:
                        #     print(f"{key}: {sensor_data[key]}")
                        
                        try:
                            # Use calibrated data to calculate total field strength
                            if 'calibrated' in sensor_data and all(axis in sensor_data['calibrated'] for axis in ['x', 'y', 'z']):
                                total_field = np.sqrt(
                                    sensor_data['calibrated']['x']**2 + 
                                    sensor_data['calibrated']['y']**2 + 
                                    sensor_data['calibrated']['z']**2
                                )
                                # print(f"Sensor {i} calibrated data - X: {sensor_data['calibrated']['x']:.2f}, "
                                #       f"Y: {sensor_data['calibrated']['y']:.2f}, "
                                #       f"Z: {sensor_data['calibrated']['z']:.2f}, "
                                #       f"Total field: {total_field:.2f}")
                            
                            # Display envelope data
                            if 'envelope' in sensor_data and all(axis in sensor_data['envelope'] for axis in ['x', 'y', 'z']):
                                envelope_total = np.sqrt(
                                    sensor_data['envelope']['x']**2 + 
                                    sensor_data['envelope']['y']**2 + 
                                    sensor_data['envelope']['z']**2
                                )
                                # print(f"Sensor {i} envelope data - X: {sensor_data['envelope']['x']:.2f}, "
                                #       f"Y: {sensor_data['envelope']['y']:.2f}, "
                                #       f"Z: {sensor_data['envelope']['z']:.2f}, "
                                #       f"Total field: {envelope_total:.2f}")
                        
                        except Exception as e:
                            print(f"Error processing sensor {i} data: {e}")
                
    except KeyboardInterrupt:
        print("\nProgram stopped")
        reader.close()

if __name__ == "__main__":
    main()