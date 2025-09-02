#include <Adafruit_MLX90393.h>
#include <SPI.h>
#include <Arduino.h>
#include <math.h>

// ==================== SYSTEM PARAMETERS ====================
#define NUM_SENSORS           8       // Number of MLX90393 magnetic sensors
#define SAMPLE_RATE           250     // Sampling frequency in Hz
#define ENV_SEC               0.05f   // Envelope calculation window duration in seconds

// Calibration parameters
#define CALIBRATION_WINDOW    50      // Calibration buffer window size (50 samples)
#define STD_THRESHOLD         3.0f    // Standard deviation threshold for calibration stability

// Moving average filter parameters
#define FILTER_WINDOW_SIZE    15      // Moving average filter window size (15 samples)

// Peak detection parameters
#define SLOPE_THRESH          0.16f   // Slope threshold for peak detection
#define AMP_THRESH            5.0f    // Amplitude threshold above baseline
#define MIN_COUNT             3       // Minimum consecutive samples to confirm peak start/end

// Derived constants
const int ENV_POINTS = int(ENV_SEC * SAMPLE_RATE);  // Envelope window size (~12 points)
const int HALF_ENV   = ENV_POINTS / 2;

// ==================== MODEL INTERFACE ====================
// External C function for neural network inference
extern "C" {
    void predict_keypress(float input[24], int* predicted_label, float* confidence);
}

// Key label mapping for 49 classes (0-48)
const char* label_names[] = {
    "'", ",", "-", ".", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ";", "A", "Alt", "B", "C", "CapsLock", 
    "Ctrl", "D", "E", "Esc", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "OS", "P", "Q", "R", "S", "Shift", 
    "Space", "T", "U", "V", "W", "/", "X", "Y", "Z"
};

// ==================== HARDWARE CONFIGURATION ====================
Adafruit_MLX90393 sensor[NUM_SENSORS];
int CS_PINS[NUM_SENSORS] = {5, 7, 9, 10, 11, 15, 27, 28};  // SPI chip select pins

// ==================== DATA STRUCTURES ====================
// Raw sensor data arrays
float raw_data[NUM_SENSORS][3];         // Raw magnetic field readings (X, Y, Z)
float filtered_data[NUM_SENSORS][3];    // Moving average filtered data
float calibrated_data[NUM_SENSORS][3];  // Offset-corrected data
float filt_data[NUM_SENSORS][3];        // Envelope data (final processed values)
float offset[NUM_SENSORS][3];           // Calibration offset values
bool  isCalibrated = false;             // System calibration status flag

// Moving average filter circular buffers
float filter_buf[NUM_SENSORS][3][FILTER_WINDOW_SIZE];
int   filter_idx[NUM_SENSORS][3] = {{0}};
int   filter_count[NUM_SENSORS][3] = {{0}};  // Data count for each axis

// Calibration circular buffers for stability detection
float calib_buf[NUM_SENSORS][3][CALIBRATION_WINDOW];
int   calib_idx[NUM_SENSORS][3] = {{0}};
int   calib_cnt[NUM_SENSORS] = {0};       // Calibration sample count per sensor

// Envelope calculation circular buffers
float env_buf[NUM_SENSORS][3][ENV_POINTS];
int   env_idx[NUM_SENSORS][3] = {{0}};

// Peak detection state structure
struct PeakState {
  bool  inPeak;         // Currently in peak detection state
  int   risingCnt;      // Consecutive rising slope count
  int   fallingCnt;     // Consecutive falling slope count
  float baseLevel;      // Dynamic baseline level
  float maxVal;         // Maximum value during current peak
  float maxData[8][3];  // Sensor data at peak maximum moment
} peak[NUM_SENSORS];

// System counters
unsigned long dataCounter = 0;

// ==================== SIGNAL PROCESSING FUNCTIONS ====================

/**
 * @brief Apply moving average filter to sensor data
 * @param sensor_id Sensor index (0-7)
 * @param axis Axis index (0=X, 1=Y, 2=Z)
 * @return Filtered value or current value if insufficient data
 */
float apply_moving_average_filter(int sensor_id, int axis) {
  int idx = filter_idx[sensor_id][axis];
  int count = filter_count[sensor_id][axis];
  
  if (count >= FILTER_WINDOW_SIZE) {
    // Calculate moving average over full window
    float sum = 0;
    for (int i = 0; i < FILTER_WINDOW_SIZE; i++) {
      sum += filter_buf[sensor_id][axis][i];
    }
    return sum / FILTER_WINDOW_SIZE;
  } else {
    // Insufficient data, return most recent value
    return filter_buf[sensor_id][axis][(idx - 1 + FILTER_WINDOW_SIZE) % FILTER_WINDOW_SIZE];
  }
}

/**
 * @brief Calculate standard deviation of buffer data
 * @param buf Pointer to data buffer
 * @param n Number of samples in buffer
 * @return Standard deviation value
 */
float compute_std(float *buf, int n) {
  float sum = 0, sumsq = 0;
  for (int i = 0; i < n; i++) {
    sum += buf[i];
    sumsq += buf[i] * buf[i];
  }
  float mean = sum / n;
  float var = sumsq / n - mean * mean;
  return sqrt(var);
}

/**
 * @brief Check if sensor calibration data is stable
 * @param sensor_id Sensor index to check
 * @return true if all axes have stable data (std < threshold)
 */
bool check_calibration_stable(int sensor_id) {
  for (int ax = 0; ax < 3; ax++) {
    if (calib_cnt[sensor_id] < CALIBRATION_WINDOW) {
      return false;
    }
    if (compute_std(calib_buf[sensor_id][ax], CALIBRATION_WINDOW) > STD_THRESHOLD) {
      return false;
    }
  }
  return true;
}

/**
 * @brief Calculate offset values for a calibrated sensor
 * @param sensor_id Sensor index to calculate offsets for
 */
void calculate_offsets(int sensor_id) {
  for (int ax = 0; ax < 3; ax++) {
    float sum = 0;
    for (int k = 0; k < CALIBRATION_WINDOW; k++) {
      sum += calib_buf[sensor_id][ax][k];
    }
    offset[sensor_id][ax] = sum / CALIBRATION_WINDOW;
  }
}

// ==================== MACHINE LEARNING INFERENCE ====================

/**
 * @brief Predict keypress from peak sensor data using neural network
 * @param peak_data 8x3 array of sensor envelope data at peak maximum
 */
void predict_keypress_from_peak_data(float peak_data[8][3]) {
  // Reshape 8x3 peak data into 24-element feature array
  float input[24];
  int idx = 0;
  
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 3; j++) {
      input[idx++] = peak_data[i][j];
    }
  }
  
  // Perform neural network inference
  int predicted_label;
  float confidence;
  
  unsigned long start_time = micros();
  predict_keypress(input, &predicted_label, &confidence);
  unsigned long end_time = micros();
  
  // Display inference results
  Serial.println("Key detected!");
  
  // Print 8x3 peak data for analysis
  Serial.println("Peak maximum moment 8x3 data:");
  for (int i = 0; i < 8; i++) {
    Serial.print("Sensor"); Serial.print(i+1); Serial.print(": ");
    Serial.print("X="); Serial.print(peak_data[i][0], 3); Serial.print(" ");
    Serial.print("Y="); Serial.print(peak_data[i][1], 3); Serial.print(" ");
    Serial.print("Z="); Serial.print(peak_data[i][2], 3);
    Serial.println();
  }
  
  if (predicted_label >= 0 && predicted_label < 49) {
    Serial.print("Predicted key: ");
    Serial.println(label_names[predicted_label]);
    Serial.print("Confidence: ");
    Serial.println(confidence, 4);
  } else {
    Serial.println("Predicted key: Unknown");
  }
  
  Serial.print("Inference time: ");
  Serial.print(end_time - start_time);
  Serial.println(" us");
  Serial.println("--------------------");
}

// ==================== SYSTEM INITIALIZATION ====================

void setup() {
  Serial.begin(115200);
  while (!Serial) delayMicroseconds(10);
  delay(1000);

  Serial.println("Initializing magnetic sensors...");
  
  // Initialize each MLX90393 sensor
  for (int i = 0; i < NUM_SENSORS; i++) {
    sensor[i] = Adafruit_MLX90393();
    while (!sensor[i].begin_SPI(CS_PINS[i])) {
      Serial.print("Sensor "); Serial.print(i+1); Serial.println(" not found - check wiring");
      delay(500);
    }
    sensor[i].setOversampling(MLX90393_OSR_0);
    sensor[i].setFilter(MLX90393_FILTER_2);

    // Initialize peak detection state
    peak[i] = {false, 0, 0, 0.0f, 0.0f, {{0}}};
    
    Serial.print("Sensor "); Serial.print(i+1); Serial.println(" initialized");
  }
  
  Serial.println("Starting calibration - collecting stable data...");
}

// ==================== MAIN PROCESSING LOOP ====================

void loop() {
  // Trigger simultaneous measurements on all sensors
  for(int i = 0; i < NUM_SENSORS; i++) {
    sensor[i].startSingleMeasurement();
  }
  delayMicroseconds(mlx90393_tconv[2][0]*1000 + 200);

  // Read measurement results from all sensors
  for(int i = 0; i < NUM_SENSORS; i++) {
    if (!sensor[i].readMeasurement(&raw_data[i][0], &raw_data[i][1], &raw_data[i][2])) {
      raw_data[i][0] = raw_data[i][1] = raw_data[i][2] = 0.0f;
    }
  }

  // Step 1: Apply moving average filtering
  for (int i = 0; i < NUM_SENSORS; i++) {
    for (int ax = 0; ax < 3; ax++) {
      // Update filter circular buffer
      filter_buf[i][ax][filter_idx[i][ax]] = raw_data[i][ax];
      filter_idx[i][ax] = (filter_idx[i][ax] + 1) % FILTER_WINDOW_SIZE;
      if (filter_count[i][ax] < FILTER_WINDOW_SIZE) {
        filter_count[i][ax]++;
      }
      
      // Apply moving average filter
      filtered_data[i][ax] = apply_moving_average_filter(i, ax);
    }
  }

  // Step 2: Calibration phase using stability detection
  if (!isCalibrated) {
    bool allCalibrated = true;
    
    for (int i = 0; i < NUM_SENSORS; i++) {
      // Fill calibration circular buffer
      for (int ax = 0; ax < 3; ax++) {
        calib_buf[i][ax][calib_idx[i][ax]] = filtered_data[i][ax];
        calib_idx[i][ax] = (calib_idx[i][ax] + 1) % CALIBRATION_WINDOW;
      }
      if (calib_cnt[i] < CALIBRATION_WINDOW) {
        calib_cnt[i]++;
      }
      
      // Check if calibration buffer is full
      if (calib_cnt[i] < CALIBRATION_WINDOW) {
        allCalibrated = false;
        continue;
      }
      
      // Check data stability (standard deviation < threshold)
      bool stable = check_calibration_stable(i);
      if (!stable) {
        allCalibrated = false;
        continue;
      }
      
      // Calculate offset if sensor is stable and not yet calibrated
      if (offset[i][0] == 0 && offset[i][1] == 0 && offset[i][2] == 0) {
        calculate_offsets(i);
        Serial.print("Sensor"); Serial.print(i+1); Serial.println(" calibrated");
      }
    }
    
    if (allCalibrated) {
      // Verify all sensors have calculated offsets
      bool allOffsetsCalculated = true;
      for (int i = 0; i < NUM_SENSORS; i++) {
        if (offset[i][0] == 0 && offset[i][1] == 0 && offset[i][2] == 0) {
          allOffsetsCalculated = false;
          break;
        }
      }
      
      if (allOffsetsCalculated) {
        isCalibrated = true;
        Serial.println("All sensors calibrated successfully!");
        
        // Display calculated offset values
        Serial.println("Sensor offset values:");
        for (int i = 0; i < NUM_SENSORS; i++) {
          Serial.print("Sensor"); Serial.print(i+1); Serial.print(": ");
          Serial.print("X="); Serial.print(offset[i][0], 3); Serial.print(" ");
          Serial.print("Y="); Serial.print(offset[i][1], 3); Serial.print(" ");
          Serial.print("Z="); Serial.print(offset[i][2], 3);
          Serial.println();
        }
        Serial.println("Starting keypress detection...");
      }
    }
    return;
  }

  // Step 3: Apply offset correction, envelope calculation, and peak detection
  for(int i = 0; i < NUM_SENSORS; i++) {
    float totalField;

    // Apply offset correction to filtered data
    for(int ax = 0; ax < 3; ax++) {
      calibrated_data[i][ax] = filtered_data[i][ax] - offset[i][ax];

      // Update envelope calculation circular buffer
      env_buf[i][ax][env_idx[i][ax]] = calibrated_data[i][ax];
      env_idx[i][ax] = (env_idx[i][ax] + 1) % ENV_POINTS;

      // Calculate envelope (find original value with maximum absolute value)
      float max_abs_value = 0;
      float envelope_value = 0;
      for(int k = 0; k < ENV_POINTS; k++) {
        float abs_val = fabs(env_buf[i][ax][k]);
        if (abs_val > max_abs_value) {
          max_abs_value = abs_val;
          envelope_value = env_buf[i][ax][k];  // Preserve original sign
        }
      }
      filt_data[i][ax] = envelope_value;  // Store envelope data
    }
    
    // Calculate total magnetic field magnitude (vector norm)
    totalField = sqrt(
      filt_data[i][0]*filt_data[i][0] +
      filt_data[i][1]*filt_data[i][1] +
      filt_data[i][2]*filt_data[i][2]
    );

    // Update dynamic baseline level (approximate 10th percentile)
    static float recent[NUM_SENSORS][50];
    static int   rcIdx[NUM_SENSORS] = {0};
    recent[i][rcIdx[i]] = totalField;
    rcIdx[i] = (rcIdx[i] + 1) % 50;
    
    float base = recent[i][0];
    for(int k = 1; k < 50; k++) {
      if (recent[i][k] < base) base = recent[i][k];
    }
    peak[i].baseLevel = base;

    // Peak detection algorithm
    static float prevVal[NUM_SENSORS] = {0};
    float slope = totalField - prevVal[i];  // Calculate slope (first derivative)
    prevVal[i] = totalField;
    
    bool isRise = (slope > SLOPE_THRESH);                          // Rising slope detected
    bool isFall = (slope < -SLOPE_THRESH);                         // Falling slope detected
    bool above  = (totalField > peak[i].baseLevel + AMP_THRESH);   // Above amplitude threshold

    if (!peak[i].inPeak) {
      // Not in peak: detect peak start condition
      if (isRise && above) {
        peak[i].risingCnt++;
        if (peak[i].risingCnt >= MIN_COUNT) {
          // Consecutive rising samples above threshold confirms peak start
          peak[i].inPeak = true;
          peak[i].maxVal = totalField;
          peak[i].risingCnt = 0;  // Reset counter
          
          // Store current sensor data as initial peak maximum
          for (int j = 0; j < NUM_SENSORS; j++) {
            for (int ax = 0; ax < 3; ax++) {
              peak[i].maxData[j][ax] = filt_data[j][ax];
            }
          }
        }
      } else {
        peak[i].risingCnt = 0;  // Reset rising counter
      }
    } else {
      // In peak: track maximum and detect peak end
      if (totalField > peak[i].maxVal) {
        peak[i].maxVal = totalField;
        // Update sensor data at new peak maximum
        for (int j = 0; j < NUM_SENSORS; j++) {
          for (int ax = 0; ax < 3; ax++) {
            peak[i].maxData[j][ax] = filt_data[j][ax];
          }
        }
      }
      
      if (isFall) {
        peak[i].fallingCnt++;
        if (peak[i].fallingCnt >= MIN_COUNT && !above) {
          // Consecutive falling samples below threshold confirms peak end
          // Trigger keypress prediction using peak maximum data
          predict_keypress_from_peak_data(peak[i].maxData);
          
          peak[i].inPeak = false;
          peak[i].fallingCnt = 0;
        }
      } else {
        peak[i].fallingCnt = 0;  // Reset falling counter
      }
    }
  }

  dataCounter++;
}
