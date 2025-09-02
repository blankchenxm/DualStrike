import pandas as pd
import numpy as np
# import matplotlib.pyplot as plt  # Removed for batch processing
from scipy.signal import savgol_filter
import os

# =============================================================================
# CONFIGURATION PARAMETERS
# =============================================================================

# Folder path containing the magnetic field data files
FOLDER_PATH = r'wooting'  # This will look for wooting folder in the same directory as this script

# Filter parameters
FILTER_TYPE = 'savgol'  # 'savgol' or 'moving_average'
FILTER_WINDOW = 15
POLY_ORDER = 3

# Offset calculation parameters
OFFSET_WINDOW_SIZE = 25
OFFSET_STD_THRESHOLD = 3

# Envelope calculation parameters
ENVELOPE_WINDOW_SIZE_SECONDS = 0.05

# Peak detection parameters
SLOPE_THRESHOLD = 0.1
AMPLITUDE_THRESHOLD = 1.5
DETECTION_WINDOW_SIZE = 0.1

# =============================================================================
# DATA PROCESSING FUNCTIONS
# =============================================================================

def read_magnetic_data(file_path, filter_type='savgol', window=15, poly_order=3):
    """
    Read magnetic field data from CSV file and apply filtering to all 8 sensors.
    
    Parameters:
    -----------
    file_path : str
        Path to the CSV file containing magnetic field data
    filter_type : str
        Type of filter to apply ('savgol' or 'moving_average')
    window : int
        Window size for filtering
    poly_order : int
        Polynomial order for Savitzky-Golay filter
        
    Returns:
    --------
    dict
        Dictionary containing filtered data for all 8 sensors
    """
    df = pd.read_csv(file_path)
    
    # Calculate relative time in seconds
    df['Time(s)'] = df['timestamp'] - df['timestamp'].iloc[0]
    
    # Create dictionary to store filtered data for all sensors
    all_sensors_data = {}
    
    # Process data for 8 sensors
    for sensor_num in range(1, 9):
        filtered_data = {}
        for axis in ['x', 'y', 'z']:
            raw_data = df[f'sensor_{sensor_num}_{axis}']
            
            if filter_type == 'savgol':
                filtered_data[axis] = savgol_filter(raw_data, window, poly_order)
            elif filter_type == 'moving_average':
                filtered_data[axis] = raw_data.rolling(window=window, center=True).mean()
                filtered_data[axis] = filtered_data[axis].fillna(method='bfill').fillna(method='ffill')
        
        # Create DataFrame for each sensor
        sensor_data = pd.DataFrame({
            'Time(s)': df['Time(s)'],
            'MagX_Raw': filtered_data['x'],
            'MagY_Raw': filtered_data['y'],
            'MagZ_Raw': filtered_data['z']
        })
        
        all_sensors_data[f'sensor_{sensor_num}'] = sensor_data
    
    print(f"Applied {filter_type} filter with window size: {window}")
    return all_sensors_data

def calculate_offset(data, window_size=25, std_threshold=3):
    """
    Calculate sensor offset values by finding the first stable segment.
    
    Parameters:
    -----------
    data : pandas.DataFrame
        Sensor data containing magnetic field measurements
    window_size : int
        Size of the sliding window for stability analysis
    std_threshold : float
        Standard deviation threshold for determining stable segments
        
    Returns:
    --------
    dict
        Dictionary containing offset values for X, Y, Z axes
    """
    offset = {'X': 0, 'Y': 0, 'Z': 0}
    
    for axis in ['X', 'Y', 'Z']:
        column = f'Mag{axis}_Raw'
        
        # Search for the first stable segment
        for i in range(len(data) - window_size):
            window = data[column][i:i+window_size]
            window_std = window.std()
            
            if window_std < std_threshold:
                offset[axis] = window.mean()
                break
        else:
            print(f"Warning: No stable segment found for {axis}-axis, using mean of first {window_size} points")
            offset[axis] = data[column][:window_size].mean()
    
    return offset

def calculate_magnetic_field(data, offset):
    """
    Subtract offset from raw magnetic field data and convert to microTesla (uT).
    
    Parameters:
    -----------
    data : pandas.DataFrame
        Raw magnetic field data
    offset : dict
        Offset values for X, Y, Z axes
        
    Returns:
    --------
    pandas.DataFrame
        Processed magnetic field data with offset correction
    """
    gain = 1.0
    
    # Apply offset correction and convert to uT
    data['MagX (uT)'] = (data['MagX_Raw'] - offset['X']) / gain
    data['MagY (uT)'] = (data['MagY_Raw'] - offset['Y']) / gain
    data['MagZ (uT)'] = (data['MagZ_Raw'] - offset['Z']) / gain
    
    # Calculate total magnetic field magnitude
    data['Total Field (uT)'] = np.sqrt(
        data['MagX (uT)']**2 + 
        data['MagY (uT)']**2 + 
        data['MagZ (uT)']**2
    )
    return data

# Plotting functions removed as they are not needed for batch processing

def calculate_envelope(data, window_size_seconds=0.05):
    """
    Calculate envelope of magnetic field signals using sliding window maximum.
    
    Parameters:
    -----------
    data : pandas.DataFrame
        Processed magnetic field data
    window_size_seconds : float
        Sliding window size in seconds
        
    Returns:
    --------
    pandas.DataFrame
        Envelope data for all axes
    """
    # Calculate window size in data points (assuming 100 Hz sampling rate)
    window_points = int(window_size_seconds * 100)
    
    envelope = pd.DataFrame()
    envelope['Time(s)'] = data['Time(s)']
    
    # Calculate envelope for each axis
    for axis in ['X', 'Y', 'Z']:
        column = f'Mag{axis} (uT)'
        signal = data[column].values
        envelope_values = np.zeros_like(signal)
        
        # For each point, find the maximum absolute value within the window
        for i in range(len(signal)):
            start_idx = max(0, i - window_points//2)
            end_idx = min(len(signal), i + window_points//2)
            window = signal[start_idx:end_idx]
            
            # Find the value with maximum absolute magnitude within the window
            max_abs_idx = np.argmax(np.abs(window))
            envelope_values[i] = window[max_abs_idx]
        
        envelope[f'Envelope_{axis}'] = envelope_values
    
    # Calculate envelope for total field
    total_field = data['Total Field (uT)'].values
    total_envelope = np.zeros_like(total_field)
    
    for i in range(len(total_field)):
        start_idx = max(0, i - window_points//2)
        end_idx = min(len(total_field), i + window_points//2)
        window = total_field[start_idx:end_idx]
        max_abs_idx = np.argmax(np.abs(window))
        total_envelope[i] = window[max_abs_idx]
    
    envelope['Envelope_Total'] = total_envelope
    
    return envelope

# Additional plotting functions removed

# =============================================================================
# PEAK DETECTION FUNCTIONS
# =============================================================================

def detect_peaks_and_flats(envelope, threshold_std=5, min_flat_duration=0.05):
    """
    Detect peak and flat segments by merging rising edges, middle plateau, and falling edges.
    
    Parameters:
    -----------
    envelope : pandas.DataFrame
        Envelope data
    threshold_std : float
        Standard deviation threshold for determining flat segments
    min_flat_duration : float
        Minimum duration for flat segments in seconds
        
    Returns:
    --------
    list
        List of tuples containing (start_time, end_time, segment_type)
    """
    signal = envelope['Envelope_Total'].values
    times = envelope['Time(s)'].values
    base_level = np.percentile(signal, 10)  # Use 10th percentile as base level
    
    # Calculate local standard deviation using sliding window
    window_points = int(min_flat_duration * 100)
    local_std = np.array([np.std(signal[max(0, i-window_points//2):min(len(signal), i+window_points//2)])
                         for i in range(len(signal))])
    
    # Initialize segment detection
    segments = []
    in_peak = False
    peak_start = 0
    last_level = 'base'  # 'base', 'rising', 'high', 'falling'
    
    for i in range(1, len(signal)):
        current_value = signal[i]
        is_active = current_value > base_level + threshold_std
        
        if not in_peak and is_active:
            # Start new peak
            in_peak = True
            peak_start = i
            last_level = 'rising'
        elif in_peak:
            if last_level == 'rising':
                if local_std[i] < threshold_std:
                    last_level = 'high'
            elif last_level == 'high':
                if local_std[i] > threshold_std and signal[i] < signal[i-1]:
                    last_level = 'falling'
            elif last_level == 'falling':
                if not is_active:
                    # End current peak
                    segments.append((times[peak_start], times[i], 'peak'))
                    in_peak = False
                    last_level = 'base'
    
    # Handle last possible peak
    if in_peak:
        segments.append((times[peak_start], times[-1], 'peak'))
    
    # Add flat segments
    flat_segments = []
    last_end = times[0]
    
    for start, end, _ in segments:
        if start > last_end:
            flat_segments.append((last_end, start, 'flat'))
        last_end = end
    
    if last_end < times[-1]:
        flat_segments.append((last_end, times[-1], 'flat'))
    
    # Merge and sort all segments
    all_segments = segments + flat_segments
    all_segments.sort(key=lambda x: x[0])
    
    return all_segments

def detect_peaks_and_flats_v3(envelope, slope_threshold=0.5, amplitude_threshold=5, window_size=0.1):
    """
    Slope-based peak detection method with improved accuracy.
    
    Parameters:
    -----------
    envelope : pandas.DataFrame
        Envelope data
    slope_threshold : float
        Slope threshold for detecting rising/falling edges
    amplitude_threshold : float
        Amplitude threshold for valid peak detection
    window_size : float
        Time window for slope calculation in seconds
        
    Returns:
    --------
    list
        List of tuples containing (start_time, end_time, segment_type)
    """
    signal = envelope['Envelope_Total'].values
    times = envelope['Time(s)'].values
    fs = 1 / np.mean(np.diff(times))  # Calculate sampling rate
    window_points = int(window_size * fs)
    
    # Calculate base level
    base_level = np.percentile(signal, 10)
    
    # Calculate slope
    def calculate_slope(data, window):
        """Calculate slope using linear regression within sliding window"""
        slopes = np.zeros_like(data)
        for i in range(len(data)):
            start_idx = max(0, i - window//2)
            end_idx = min(len(data), i + window//2)
            if end_idx - start_idx > 1:
                slopes[i] = np.polyfit(range(end_idx-start_idx), 
                                     data[start_idx:end_idx], 1)[0]
        return slopes
    
    slopes = calculate_slope(signal, window_points)
    
    # Initialize segment detection
    segments = []
    in_peak = False
    peak_start = 0
    
    # State machine variables
    rising_count = 0
    falling_count = 0
    min_count = int(0.05 * fs)  # Minimum duration (50ms)
    
    for i in range(1, len(signal)):
        current_slope = slopes[i]
        current_value = signal[i]
        
        # Determine current state
        is_rising = current_slope > slope_threshold
        is_falling = current_slope < -slope_threshold
        is_above_threshold = current_value > base_level + amplitude_threshold
        
        if not in_peak:
            if is_rising and is_above_threshold:
                rising_count += 1
                if rising_count >= min_count:
                    # Confirm rising edge
                    in_peak = True
                    peak_start = i - rising_count
                    rising_count = 0
            else:
                rising_count = 0
        else:  # Currently in peak
            if is_falling:
                falling_count += 1
                if falling_count >= min_count and current_value < base_level + amplitude_threshold:
                    # Confirm falling edge
                    segments.append((times[peak_start], times[i], 'peak'))
                    in_peak = False
                    falling_count = 0
            elif not is_above_threshold:
                # Direct return to base level
                if i - peak_start > min_count:
                    segments.append((times[peak_start], times[i], 'peak'))
                in_peak = False
                falling_count = 0
    
    # Handle last possible peak
    if in_peak:
        segments.append((times[peak_start], times[-1], 'peak'))
    
    # Add flat segments
    flat_segments = []
    last_end = times[0]
    
    for start, end, _ in segments:
        if start > last_end:
            flat_segments.append((last_end, start, 'flat'))
        last_end = end
    
    if last_end < times[-1]:
        flat_segments.append((last_end, times[-1], 'flat'))
    
    # Merge and sort all segments
    all_segments = segments + flat_segments
    all_segments.sort(key=lambda x: x[0])
    
    return all_segments

def detect_peaks_and_flats_v2(envelope, prominence_threshold=5, width_threshold=0.05):
    """
    Peak detection method based on peak characteristics using scipy.signal.find_peaks.
    
    Parameters:
    -----------
    envelope : pandas.DataFrame
        Envelope data
    prominence_threshold : float
        Peak prominence threshold
    width_threshold : float
        Minimum peak width in seconds
        
    Returns:
    --------
    list
        List of tuples containing (start_time, end_time, segment_type)
    """
    from scipy.signal import find_peaks
    
    signal = envelope['Envelope_Total'].values
    times = envelope['Time(s)'].values
    fs = 1 / np.mean(np.diff(times))  # Calculate sampling rate
    
    # Use scipy's find_peaks function to detect peaks
    peaks, properties = find_peaks(signal, 
                                 prominence=prominence_threshold,
                                 width=width_threshold*fs,
                                 rel_height=0.5)
    
    # Initialize segments
    segments = []
    
    # Determine peak segments based on peak widths
    for i, peak in enumerate(peaks):
        left_idx = int(peak - properties['widths'][i])
        right_idx = int(peak + properties['widths'][i])
        
        # Ensure indices are within valid range
        left_idx = max(0, left_idx)
        right_idx = min(len(times)-1, right_idx)
        
        segments.append((times[left_idx], times[right_idx], 'peak'))
    
    # Add flat segments
    flat_segments = []
    last_end = times[0]
    
    for start, end, _ in sorted(segments):
        if start > last_end:
            flat_segments.append((last_end, start, 'flat'))
        last_end = end
    
    if last_end < times[-1]:
        flat_segments.append((last_end, times[-1], 'flat'))
    
    # Merge and sort all segments
    all_segments = segments + flat_segments
    all_segments.sort(key=lambda x: x[0])
    
    return all_segments

def calculate_peak_vectors(data, envelope, segments):
    """
    Calculate feature vectors for each peak segment.
    
    Parameters:
    -----------
    data : pandas.DataFrame
        Processed magnetic field data
    envelope : pandas.DataFrame
        Envelope data
    segments : list
        List of detected segments
        
    Returns:
    --------
    list
        List of dictionaries containing peak information and feature vectors
    """
    peak_vectors = []
    
    # Find all peak and flat segments
    peak_segments = [seg for seg in segments if seg[2] == 'peak']
    flat_segments = [seg for seg in segments if seg[2] == 'flat']
    
    for i, (start_time, end_time, _) in enumerate(peak_segments):
        # Find the previous flat segment
        prev_flat = None
        for flat in flat_segments:
            if flat[1] <= start_time:  # Find the closest flat segment before the peak
                prev_flat = flat
        
        if prev_flat is None:
            continue
            
        # Calculate mean values for the previous flat segment
        flat_mask = (data['Time(s)'] >= prev_flat[0]) & (data['Time(s)'] <= prev_flat[1])
        flat_means = {
            'X': data.loc[flat_mask, 'MagX (uT)'].mean(),
            'Y': data.loc[flat_mask, 'MagY (uT)'].mean(),
            'Z': data.loc[flat_mask, 'MagZ (uT)'].mean()
        }
        
        # Find the high plateau within the peak segment
        peak_signal = envelope.loc[(envelope['Time(s)'] >= start_time) & 
                                 (envelope['Time(s)'] <= end_time), 'Envelope_Total']
        peak_max = peak_signal.max()
        plateau_threshold = peak_max * 0.9  # Consider values above 90% of max as plateau
        
        plateau_mask = (data['Time(s)'] >= start_time) & \
                      (data['Time(s)'] <= end_time) & \
                      (envelope['Envelope_Total'] >= plateau_threshold)
        
        # Calculate peak vector using only the plateau region
        peak_means = {
            'X': data.loc[plateau_mask, 'MagX (uT)'].mean() - flat_means['X'],
            'Y': data.loc[plateau_mask, 'MagY (uT)'].mean() - flat_means['Y'],
            'Z': data.loc[plateau_mask, 'MagZ (uT)'].mean() - flat_means['Z']
        }
        
        # Calculate magnitude
        magnitude = np.sqrt(peak_means['X']**2 + peak_means['Y']**2 + peak_means['Z']**2)
        
        peak_vectors.append({
            'peak_number': i + 1,
            'start_time': start_time,
            'end_time': end_time,
            'vector': peak_means,
            'magnitude': magnitude
        })
    
    return peak_vectors

def get_peak_max_data(all_data, envelope, segments, target_sensor):
    """
    Get all sensor data at the maximum value moment of each peak segment.
    
    Parameters:
    -----------
    all_data : dict
        Dictionary containing all sensor data
    envelope : pandas.DataFrame
        Envelope data for the target sensor
    segments : list
        Segment data for the target sensor
    target_sensor : int
        Target sensor number
        
    Returns:
    --------
    list
        List containing data for all peaks at maximum value moments
    """
    peak_data_list = []
    
    # Get peak segments
    peak_segments = [seg for seg in segments if seg[2] == 'peak']
    
    for start_time, end_time, _ in peak_segments:
        # Find the moment of maximum Envelope_Total within the peak segment
        peak_mask = (envelope['Time(s)'] >= start_time) & (envelope['Time(s)'] <= end_time)
        peak_data = envelope[peak_mask]
        max_idx = peak_data['Envelope_Total'].idxmax()
        max_time = envelope.loc[max_idx, 'Time(s)']
        
        # Collect data from all sensors at this moment
        sensor_data = []
        for sensor_num in range(1, 9):  # Changed to 8 sensors
            sensor_key = f'sensor_{sensor_num}'
            data = all_data[sensor_key]
            
            # Find the closest time point
            closest_idx = (data['Time(s)'] - max_time).abs().idxmin()
            
            # Add three-axis data
            sensor_data.extend([
                data.loc[closest_idx, 'MagX (uT)'],
                data.loc[closest_idx, 'MagY (uT)'],
                data.loc[closest_idx, 'MagZ (uT)']
            ])
        
        peak_data_list.append(sensor_data)
    
    return peak_data_list

def process_single_file(file_path):
    """
    Process a single magnetic field data file and return detailed results.
    
    Parameters:
    -----------
    file_path : str
        Path to the CSV file containing magnetic field data
        
    Returns:
    --------
    dict
        Dictionary containing peak counts, max sensor info, and processed data
    """
    # Read magnetic field data from all sensors
    all_sensors_data = read_magnetic_data(file_path, 
                                        filter_type=FILTER_TYPE,
                                        window=FILTER_WINDOW,
                                        poly_order=POLY_ORDER)
    
    # Track peak counts and processed data for each sensor
    peak_counts = {}
    processed_sensors_data = {}
    envelopes = {}
    segments_data = {}
    
    # Process data for each sensor
    for sensor_num in range(1, 9):
        sensor_key = f'sensor_{sensor_num}'
        data = all_sensors_data[sensor_key]
        
        # Calculate offset values and apply correction
        offset = calculate_offset(data, 
                                window_size=OFFSET_WINDOW_SIZE,
                                std_threshold=OFFSET_STD_THRESHOLD)
        processed_data = calculate_magnetic_field(data, offset)
        
        # Calculate envelope curves
        envelope = calculate_envelope(processed_data, 
                                    window_size_seconds=ENVELOPE_WINDOW_SIZE_SECONDS)
        
        # Detect peak and flat segments
        segments = detect_peaks_and_flats_v3(envelope, 
                                            slope_threshold=SLOPE_THRESHOLD,
                                            amplitude_threshold=AMPLITUDE_THRESHOLD,
                                            window_size=DETECTION_WINDOW_SIZE)
        
        # Calculate peak feature vectors
        peak_vectors = calculate_peak_vectors(processed_data, envelope, segments)
        
        # Record peak count and store processed data
        peak_count = len(peak_vectors)
        peak_counts[sensor_num] = peak_count
        processed_sensors_data[sensor_key] = processed_data
        envelopes[sensor_key] = envelope
        segments_data[sensor_key] = segments
    
    # Find sensor with maximum peak count
    max_sensor = max(peak_counts, key=peak_counts.get)
    max_peak_count = peak_counts[max_sensor]
    
    return {
        'peak_counts': peak_counts,
        'max_sensor': max_sensor,
        'max_peak_count': max_peak_count,
        'processed_data': processed_sensors_data,
        'envelopes': envelopes,
        'segments': segments_data
    }

# =============================================================================
# MAIN PROCESSING FUNCTION
# =============================================================================

def main():
    """
    Main function to process magnetic field data and detect peaks across all sensors for all files in the folder.
    """
    # Check if folder exists
    if not os.path.exists(FOLDER_PATH):
        print(f"Error: Folder '{FOLDER_PATH}' does not exist.")
        return
    
    # Get all CSV files in the folder
    csv_files = [f for f in os.listdir(FOLDER_PATH) if f.endswith('.csv')]
    
    if not csv_files:
        print(f"No CSV files found in folder '{FOLDER_PATH}'.")
        return
    
    print(f"Processing {len(csv_files)} files in folder '{FOLDER_PATH}'...")
    print("=" * 50)
    
    # Storage for results and peak data
    key_results = {}
    all_processed_data = {}
    all_peak_data = []
    
    # Process each file
    for filename in csv_files:
        # Extract key name from filename
        # Expected format: wooting_key_[KEY].csv
        try:
            if filename.startswith('wooting_key_') and filename.endswith('.csv'):
                # Remove .csv extension first, then extract key name
                name_without_ext = filename[:-4]  # Remove '.csv'
                key = name_without_ext.split('_', 2)[2]  # Split only at first 2 underscores
                
                # Special handling: WHY actually corresponds to the / key
                if key == 'WHY':
                    key = '/'
                    
                file_path = os.path.join(FOLDER_PATH, filename)
                
                # Process the file
                result = process_single_file(file_path)
                
                # Store results
                key_results[key] = result
                all_processed_data[key] = result
                
                # Print result
                print(f"Key: {key}, Final detected peak count: {result['max_peak_count']}")
            else:
                print(f"Skipping file: {filename} (does not match expected format)")
        except Exception as e:
            print(f"Error processing file {filename}: {e}")
    
    print("=" * 50)
    print("Processing completed.")
    
    # Collect peak data from keys with sufficient peaks
    active_keys = {}
    for key, result in key_results.items():
        # Check if any sensor has >= 18 peaks (as in segment2.py)
        if any(peaks >= 18 for peaks in result['peak_counts'].values()):
            max_sensor = result['max_sensor']
            max_peaks = result['max_peak_count']
            active_keys[key] = (max_sensor, max_peaks)
            
            # Get peak data for this key
            peak_data = get_peak_max_data(
                result['processed_data'],
                result['envelopes'][f'sensor_{max_sensor}'],
                result['segments'][f'sensor_{max_sensor}'],
                max_sensor
            )
            
            # Add class labels
            for data in peak_data:
                data.append(key)
                all_peak_data.append(data)
    
    # Create DataFrame and save to CSV
    if all_peak_data:
        columns = []
        for i in range(1, 9):  # 8 sensors
            for axis in ['X', 'Y', 'Z']:
                columns.append(f'Sensor{i}_{axis}')
        columns.append('Key')
        
        df = pd.DataFrame(all_peak_data, columns=columns)
        df.to_csv('wooting_peak_data.csv', index=False)
        print(f"\nPeak data saved to 'wooting_peak_data.csv' ({len(all_peak_data)} samples)")
    
    # Print active keys information
    print("\n=== Keys with peaks >= 18 ===")
    for key, (sensor, peaks) in active_keys.items():
        print(f"Key '{key}': Sensor {sensor} has the maximum peaks ({peaks})")



if __name__ == "__main__":
    main() 