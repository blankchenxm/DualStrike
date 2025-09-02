# DualStrike Python Offline Implementation

## Description

This directory contains the Python scripts for offline processing and visualization of eavesdropping and calibration data related to DualStrike.


## Script Overview

- **serial_collect_training.py**:  
  After flashing the `2.Firmware/ReadSensor_Wired_Arduino` firmware, this script collects eavesdropping training data from the magnetometer array via serial port.

- **serial_collect_calibration.py**:  
  After flashing the same firmware, this script collects calibration data from the magnetometer array via serial port.

- **segment_single_key.py**:  
  Analyzes a CSV file recording a keystroke session (as described in Sec. V.A of the paper), segments each keystroke, identifies the actual keystroke time periods, and visualizes the results as shown in Fig. 11.

- **segment_all_key.py**:
  After collecting all keystroke data for eavesdropping, this script performs unified analysis.

- **classify.py**:  
  Trains an MLP model and outputs a `.pth` file.

- **realtime.py**:  
  Uses the `.pth` model file to perform real-time keystroke prediction on the computer.

- **train_extract.py**:  
  Extracts model parameters from the `.pth` file and converts them into a `keypress_model_weights.c` file for use in `2.Firmware/DualStrike_Arduino`.

- **calibration_process.py**:  
  Predicts the actual/expected keystrokes and confidence from calibration data, providing input for subsequent calibration algorithms.

- **calibration_WLS.py**:  
  Calculates displacement from collected calibration data and visualizes the keyboard layout as shown in Fig. 16.

## Workflow

### Eavesdropping Python Implementation

1. Flash the `2.Firmware/ReadSensor_Wired_Arduino` firmware.
2. Use `serial_collect_training.py` to collect data:  
   - Specify the serial port and current key.
   - Run the script, press the spacebar to start recording, press the key 30 times, then press the spacebar again to stop.
3. *(Optional)* Use `segment_single_key.py` to perform peak detection and locate each keystroke in the collected file (e.g., `wooting/wooting_key_Alt.csv`).
4. After collecting all keystroke training data, run `segment_all_key.py` to generate CSV files containing peak data for each key.
5. Train the model using `classify.py` and check the model accuracy.
6. Use `realtime.py` for real-time keystroke prediction.
7. Run `train_extract.py` to convert the trained model into `keypress_model_weights.c` for use in `2.Firmware/DualStrike_Arduino`.

### Calibration Python Implementation

1. Flash the `2.Firmware/ReadSensor_Wired_Arduino` firmware.
2. Use `serial_collect_calibration.py` to collect calibration data:  
   - Specify the serial port and calibration sequence.
   - With the Hall-effect keyboard connected, press each key in the calibration sequence in order to record sensor data.
3. Run `calibration_process.py` to predict the actual/expected keystrokes and confidence from the calibration data, providing input for the calibration algorithm.
4. Run `calibration_WLS.py` to calculate the displacement.
