import time
import numpy as np
from collections import deque
from datetime import datetime
import pandas as pd

import torch
import torch.nn as nn
import numpy as np
import matplotlib.pyplot as plt
from classify import KeypressClassifier

##- eavesdrop1.csv: Keystroke content is "this is dualstrike."
##- eavesdrop2.csv: Keystroke content is "abcd...xyz"

# Configuration constants
CSV_FILE_PATH = "3.Software/Data/keystroke_eavesdrop/eavesdrop1.csv"
MODEL_PATH = "3.Software/wooting_keypress_model2.pth"

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
        checkpoint = torch.load(model_path, map_location=self.device, weights_only=False)
        
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


class OfflineEavesdropProcessor:
    """
    Offline processing of magnetic field data from CSV files, detecting peaks and predicting keystroke sequences
    """
    
    def __init__(self, csv_file_path=CSV_FILE_PATH, model_path=MODEL_PATH):
        """
        Initialize offline processor
        
        Args:
            csv_file_path (str): Path to CSV data file
            model_path (str): Path to pre-trained model
        """
        self.csv_file_path = csv_file_path
        self.model_path = model_path
        
        # Processing parameters
        self.NUM_SENSORS = 8
        self.FILTER_WINDOW_SIZE = 15
        self.CALIBRATION_WINDOW_SIZE = 50
        self.CALIBRATION_STD_THRESHOLD = 3
        self.CALIBRATION_BUFFER_MIN_SIZE = 20
        self.SAMPLING_RATE = 250
        self.ENVELOPE_WINDOW_SECONDS = 0.05
        self.PEAK_SLOPE_THRESHOLD = 0.16
        self.PEAK_AMPLITUDE_THRESHOLD = 5
        self.PEAK_MIN_DURATION_COUNT = 3
        self.PEAK_TIMEOUT_SECONDS = 0.012
        self.PEAK_VALUES_BUFFER_SIZE = 50
        self.PEAK_BASE_PERCENTILE = 10
        
        # Calculate derived parameters
        self.envelope_window_points = int(self.ENVELOPE_WINDOW_SECONDS * self.SAMPLING_RATE)
        self.envelope_half_window = self.envelope_window_points // 2
        
        # Initialize prediction model
        try:
            self.predictor = KeypressPredictor(model_path)
            print("Successfully loaded keypress prediction model")
        except Exception as e:
            print(f"Failed to load prediction model: {e}")
            self.predictor = None

        # Data storage
        self.raw_data = None
        self.processed_data = []
        self.detected_peaks = []
        self.predicted_keys = []
        self.keystroke_sequence = []
        self.complete_sequence = ""
        
    def load_csv_data(self):
        """Load CSV data file"""
        print(f"Loading CSV file: {self.csv_file_path}")
        self.raw_data = pd.read_csv(self.csv_file_path)
        print(f"Data loaded successfully, {len(self.raw_data)} rows")
        return self.raw_data
    
    def apply_moving_average_filter(self, data_series, window_size=None):
        """Apply moving average filter to data series"""
        if window_size is None:
            window_size = self.FILTER_WINDOW_SIZE
        
        filtered_data = []
        for i in range(len(data_series)):
            start_idx = max(0, i - window_size + 1)
            end_idx = i + 1
            filtered_data.append(np.mean(data_series[start_idx:end_idx]))
        
        return np.array(filtered_data)
    
    def calculate_offsets(self, data_series, window_size=None):
        """Calculate calibration offsets"""
        if window_size is None:
            window_size = self.CALIBRATION_WINDOW_SIZE
        
        # Use initial data to calculate offset
        calibration_data = data_series[:window_size]
        if np.std(calibration_data) <= self.CALIBRATION_STD_THRESHOLD:
            return np.mean(calibration_data)
        else:
            # If data is unstable, use more data
            extended_window = min(len(data_series) // 10, window_size * 3)
            return np.mean(data_series[:extended_window])
    
    def calculate_envelope(self, data_series):
        """Calculate envelope"""
        envelope_data = []
        
        for i in range(len(data_series)):
            # Calculate sliding window
            start_idx = max(0, i - self.envelope_half_window)
            end_idx = min(len(data_series), i + self.envelope_half_window + 1)
            window_data = data_series[start_idx:end_idx]
            
            # Find maximum absolute value in window
            max_abs_value = np.max(np.abs(window_data))
            # Keep original sign
            max_idx = np.argmax(np.abs(window_data))
            envelope_data.append(window_data[max_idx])
        
        return np.array(envelope_data)
    
    def process_data(self):
        """Process all data: filtering, calibration, envelope calculation"""
        print("Starting data processing...")
        
        if self.raw_data is None:
            self.load_csv_data()
        
        # Process data for each sensor and each axis
        processed_sensors = {}
        
        for sensor_id in range(1, self.NUM_SENSORS + 1):
            sensor_data = {}
            
            for axis in ['x', 'y', 'z']:
                col_name = f'sensor_{sensor_id}_{axis}'
                raw_values = self.raw_data[col_name].values
                
                # Apply moving average filtering
                filtered_values = self.apply_moving_average_filter(raw_values)
                
                # Calculate offset
                offset = self.calculate_offsets(filtered_values)
                
                # Apply calibration
                calibrated_values = filtered_values - offset
                
                # Calculate envelope
                envelope_values = self.calculate_envelope(calibrated_values)
                
                sensor_data[axis] = {
                    'raw': raw_values,
                    'filtered': filtered_values,
                    'calibrated': calibrated_values,
                    'envelope': envelope_values
                }
            
            processed_sensors[f'sensor_{sensor_id}'] = sensor_data
        
        # Build processed data structure
        for i in range(len(self.raw_data)):
            data_point = {
                'counter': self.raw_data.iloc[i]['counter'],
                'timestamp': self.raw_data.iloc[i]['timestamp'],
                'sensors': []
            }
            
            for sensor_id in range(1, self.NUM_SENSORS + 1):
                sensor_info = {
                    'raw': {},
                    'filtered': {},
                    'calibrated': {},
                    'envelope': {}
                }
                
                for axis in ['x', 'y', 'z']:
                    sensor_data = processed_sensors[f'sensor_{sensor_id}'][axis]
                    sensor_info['raw'][axis] = sensor_data['raw'][i]
                    sensor_info['filtered'][axis] = sensor_data['filtered'][i]
                    sensor_info['calibrated'][axis] = sensor_data['calibrated'][i]
                    sensor_info['envelope'][axis] = sensor_data['envelope'][i]
                
                data_point['sensors'].append(sensor_info)
            
            self.processed_data.append(data_point)
        
        print("Data processing completed")
        return self.processed_data
    
    def detect_peaks_offline(self):
        """Offline peak detection"""
        print("Starting peak detection...")
        
        if not self.processed_data:
            self.process_data()
        
        # Initialize peak detection status for each sensor
        peak_status = {}
        for sensor_id in range(1, self.NUM_SENSORS + 1):
            peak_status[f'sensor_{sensor_id}'] = {
                'in_peak': False,
                'peak_start_idx': 0,
                'rising_count': 0,
                'falling_count': 0,
                'base_level': 0,
                'values_buffer': deque(maxlen=self.PEAK_VALUES_BUFFER_SIZE),
                'max_value': 0,
                'max_idx': 0,
                'max_data': None
            }
        
        # Iterate through all data points for peak detection
        for data_idx, data_point in enumerate(self.processed_data):
            for sensor_id in range(1, self.NUM_SENSORS + 1):
                sensor_data = data_point['sensors'][sensor_id - 1]
                
                                # Calculate total field strength
                envelope_total = np.sqrt(
                    sensor_data['envelope']['x']**2 + 
                    sensor_data['envelope']['y']**2 + 
                    sensor_data['envelope']['z']**2
                )
                
                status = peak_status[f'sensor_{sensor_id}']
                status['values_buffer'].append(envelope_total)
                
                # Dynamically update baseline level
                if len(status['values_buffer']) >= self.CALIBRATION_BUFFER_MIN_SIZE:
                    status['base_level'] = np.percentile(list(status['values_buffer']), self.PEAK_BASE_PERCENTILE)
                
                # Calculate slope
                values = list(status['values_buffer'])
                if len(values) >= 2:
                    current_slope = values[-1] - values[-2]
                else:
                    continue
                
                # Determine signal characteristics
                is_rising = current_slope > self.PEAK_SLOPE_THRESHOLD
                is_falling = current_slope < -self.PEAK_SLOPE_THRESHOLD
                is_above_threshold = envelope_total > status['base_level'] + self.PEAK_AMPLITUDE_THRESHOLD
                
                # Peak detection state machine
                if not status['in_peak']:
                    # Look for peak start
                    if is_rising and is_above_threshold:
                        status['rising_count'] += 1
                        if status['rising_count'] >= self.PEAK_MIN_DURATION_COUNT:
                            status['in_peak'] = True
                            status['peak_start_idx'] = data_idx
                            status['rising_count'] = 0
                            status['max_value'] = 0
                            status['max_idx'] = data_idx
                            status['max_data'] = None
                            print(f"Sensor {sensor_id} peak start detected (index {data_idx})")
                else:
                    # During peak: track maximum value
                    if envelope_total > status['max_value']:
                        status['max_value'] = envelope_total
                        status['max_idx'] = data_idx
                        status['max_data'] = data_point
                    
                    # Look for peak end
                    if is_falling:
                        status['falling_count'] += 1
                        if status['falling_count'] >= self.PEAK_MIN_DURATION_COUNT and not is_above_threshold:
                            # Peak ended
                            peak_duration = data_idx - status['peak_start_idx']
                            print(f"Sensor {sensor_id} peak end detected (index {data_idx}), duration: {peak_duration} samples")
                            
                            # Record peak
                            peak_info = {
                                'sensor_id': sensor_id,
                                'start_idx': status['peak_start_idx'],
                                'end_idx': data_idx,
                                'max_idx': status['max_idx'],
                                'max_value': status['max_value'],
                                'max_data': status['max_data'],
                                'duration': peak_duration
                            }
                            self.detected_peaks.append(peak_info)
                            
                            # Reset state
                            status['in_peak'] = False
                            status['falling_count'] = 0
                    elif not is_above_threshold:
                        # Peak ended by returning to baseline
                        peak_duration = data_idx - status['peak_start_idx']
                        if peak_duration > self.PEAK_MIN_DURATION_COUNT:
                            print(f"Sensor {sensor_id} peak end (baseline return, index {data_idx}), duration: {peak_duration} samples")
                            
                            peak_info = {
                                'sensor_id': sensor_id,
                                'start_idx': status['peak_start_idx'],
                                'end_idx': data_idx,
                                'max_idx': status['max_idx'],
                                'max_value': status['max_value'],
                                'max_data': status['max_data'],
                                'duration': peak_duration
                            }
                            self.detected_peaks.append(peak_info)
                        
                        status['in_peak'] = False
                        status['falling_count'] = 0

        print(f"Peak detection completed, {len(self.detected_peaks)} peaks detected")
        return self.detected_peaks
    
    def predict_keys(self):
        """Predict keys for detected peaks"""
        print("Starting key prediction...")
        
        if not self.detected_peaks:
            self.detect_peaks_offline()
        
        if self.predictor is None:
            print("Prediction model not loaded, skipping key prediction")
            return []
        
        # Merge nearby peaks (likely multiple sensor responses from same keypress)
        merged_peaks = self._merge_nearby_peaks()
        
        for peak_group in merged_peaks:
            # Use peak maximum moment data for prediction
            max_data = peak_group['max_data']
            if max_data:
                # Build 8x3 prediction data
                peak_data = np.zeros((8, 3))
                for i, sensor_data in enumerate(max_data['sensors']):
                    peak_data[i] = [
                        sensor_data['envelope']['x'],
                        sensor_data['envelope']['y'],
                        sensor_data['envelope']['z']
                    ]
                
                # Perform prediction
                try:
                    predicted_label, probability = self.predictor.predict(peak_data)
                    peak_group['predicted_key'] = predicted_label
                    peak_group['probability'] = probability
                    print(f"Peak group (index {peak_group['start_idx']}-{peak_group['end_idx']}): predicted key = {predicted_label}, probability = {probability:.2f}")
                except Exception as e:
                    print(f"Prediction error: {e}")
                    peak_group['predicted_key'] = 'Unknown'
                    peak_group['probability'] = 0.0
        
        self.predicted_keys = merged_peaks
        
        # Generate keystroke sequence in chronological order
        self.keystroke_sequence = []
        for peak_group in sorted(self.predicted_keys, key=lambda x: x['start_idx']):
            key = peak_group['predicted_key'].lower()
            # Handle special keys
            if key == 'space':
                self.keystroke_sequence.append(' ')
            else:
                self.keystroke_sequence.append(key)
        
        # Create complete sentence string
        self.complete_sequence = ''.join(self.keystroke_sequence)
        
        print(f"Key prediction completed, {len(self.predicted_keys)} keys predicted")
        print(f"Complete keystroke sequence: \"{self.complete_sequence}\"")
        return self.predicted_keys
    
    def _merge_nearby_peaks(self, time_threshold=0.1):
        """Merge temporally nearby peaks (likely same keypress)"""
        if not self.detected_peaks:
            return []
        
        # Sort peaks by time
        sorted_peaks = sorted(self.detected_peaks, key=lambda x: x['start_idx'])
        
        merged_groups = []
        current_group = [sorted_peaks[0]]
        
        for i in range(1, len(sorted_peaks)):
            current_peak = sorted_peaks[i]
            last_peak_in_group = current_group[-1]
            
            # Calculate time difference (based on sampling rate)
            time_diff = (current_peak['start_idx'] - last_peak_in_group['end_idx']) / self.SAMPLING_RATE
            
            if time_diff <= time_threshold:
                # Merge into current group
                current_group.append(current_peak)
            else:
                # Complete current group, start new group
                merged_groups.append(self._create_merged_peak_group(current_group))
                current_group = [current_peak]
        
        # Add last group
        if current_group:
            merged_groups.append(self._create_merged_peak_group(current_group))
        
        return merged_groups
    
    def _create_merged_peak_group(self, peak_group):
        """Create merged peak group information"""
        start_idx = min(peak['start_idx'] for peak in peak_group)
        end_idx = max(peak['end_idx'] for peak in peak_group)
        
        # Find peak with maximum value
        max_peak = max(peak_group, key=lambda x: x['max_value'])
        
        return {
            'start_idx': start_idx,
            'end_idx': end_idx,
            'max_idx': max_peak['max_idx'],
            'max_value': max_peak['max_value'],
            'max_data': max_peak['max_data'],
            'peak_count': len(peak_group),
            'sensor_ids': [peak['sensor_id'] for peak in peak_group],
            'predicted_key': None,
            'probability': 0.0
        }
    
    def plot_results(self):
        """Generate visualization chart"""
        print("Generating visualization chart...")
        
        if not self.predicted_keys:
            self.predict_keys()
        
        # Prepare plot data
        fig, ax = plt.subplots(figsize=(16, 10))
        
        # Calculate time axis (based on sampling rate)
        time_axis = np.arange(len(self.processed_data)) / self.SAMPLING_RATE
        
        # Plot total field strength for each sensor
        colors = plt.cm.tab10(np.linspace(0, 1, self.NUM_SENSORS))
        
        for sensor_id in range(1, self.NUM_SENSORS + 1):
            total_fields = []
            for data_point in self.processed_data:
                sensor_data = data_point['sensors'][sensor_id - 1]
                total_field = np.sqrt(
                            sensor_data['envelope']['x']**2 + 
                            sensor_data['envelope']['y']**2 + 
                            sensor_data['envelope']['z']**2
                        )
                total_fields.append(total_field)
            
            ax.plot(time_axis, total_fields, 
                   label=f'Sensor {sensor_id}', 
                   alpha=0.6, 
                   color=colors[sensor_id-1],
                   linewidth=1.5)
        
        # Mark predicted keys with improved visualization
        ymin, ymax = ax.get_ylim()
        y_range = ymax - ymin
        
        # Use different colors for each peak
        peak_colors = plt.cm.Set3(np.linspace(0, 1, len(self.predicted_keys)))
        
        for i, key_info in enumerate(self.predicted_keys):
            start_time = key_info['start_idx'] / self.SAMPLING_RATE
            end_time = key_info['end_idx'] / self.SAMPLING_RATE
            center_time = (start_time + end_time) / 2
            
            # Add peak region highlighting with unique color
            ax.axvspan(start_time, end_time, 
                      color=peak_colors[i], 
                      alpha=0.3, 
                      edgecolor='red', 
                      linewidth=2)
            
            # Add key annotation with better positioning
            predicted_key = key_info.get('predicted_key', 'Unknown')
            probability = key_info.get('probability', 0.0)
            
            # Calculate text position to avoid overlap
            text_y = ymax - (0.15 + (i % 4) * 0.08) * y_range
            
            # Add annotation with arrow pointing to peak center
            ax.annotate(f'Key: {predicted_key}\\nProb: {probability:.2f}',
                       xy=(center_time, text_y),
                       xytext=(center_time, text_y + 0.05 * y_range),
                       ha='center', va='bottom',
                       bbox=dict(boxstyle='round,pad=0.3', 
                               facecolor='white', 
                               alpha=0.9, 
                               edgecolor='black',
                               linewidth=1),
                       fontsize=10, fontweight='bold',
                       arrowprops=dict(arrowstyle='->', 
                                     connectionstyle='arc3,rad=0',
                                     color='red', lw=1.5))
        
        # Enhance plot appearance
        ax.set_xlabel('Time (seconds)', fontsize=12, fontweight='bold')
        ax.set_ylabel('Total Magnetic Field (μT)', fontsize=12, fontweight='bold')
        ax.set_title('Offline Keystroke Detection and Prediction Results', 
                    fontsize=14, fontweight='bold', pad=20)
        ax.grid(True, alpha=0.3, linestyle='--')
        ax.legend(bbox_to_anchor=(1.02, 1), loc='upper left', fontsize=10)
        
        # Add keystroke sequence at the bottom
        if hasattr(self, 'complete_sequence') and self.complete_sequence:
            ax.text(0.5, -0.12, f'Complete Sequence: "{self.complete_sequence}"', 
                   transform=ax.transAxes, ha='center', va='top',
                   fontsize=12, fontweight='bold',
                   bbox=dict(boxstyle='round,pad=0.5', facecolor='lightblue', alpha=0.8))
        
        plt.tight_layout()
        plt.subplots_adjust(bottom=0.15)  # Make room for sequence text
        plt.show()
        
        # Print prediction results summary
        print("\\n=== Prediction Results Summary ===")
        for i, key_info in enumerate(self.predicted_keys):
            start_time = key_info['start_idx'] / self.SAMPLING_RATE
            end_time = key_info['end_idx'] / self.SAMPLING_RATE
            predicted_key = key_info.get('predicted_key', 'Unknown')
            probability = key_info.get('probability', 0.0)
            sensor_count = key_info['peak_count']
            
            print(f"Key {i+1}: {predicted_key} (probability: {probability:.2f}) "
                  f"time: {start_time:.2f}-{end_time:.2f}s "
                  f"triggered sensors: {sensor_count}")
        
        print(f"\\nComplete keystroke sequence: \"{self.complete_sequence}\"")
    
    def run_complete_analysis(self):
        """Run complete offline analysis pipeline"""
        print("Starting complete offline analysis...")
        
        # Load data
        self.load_csv_data()
        
        # Process data
        self.process_data()
        
        # Detect peaks
        self.detect_peaks_offline()
        
        # Predict keys
        self.predict_keys()
        
        # Plot results
        self.plot_results()
        
        print("Analysis completed!")

def main():
    """Main function - offline CSV data processing"""
    # Directly use offline mode to process specified CSV file
    print(f"Starting offline processing of CSV file: {CSV_FILE_PATH}")
    print(f"Using model: {MODEL_PATH}")
    processor = OfflineEavesdropProcessor()
    processor.run_complete_analysis()

if __name__ == "__main__":
    main()