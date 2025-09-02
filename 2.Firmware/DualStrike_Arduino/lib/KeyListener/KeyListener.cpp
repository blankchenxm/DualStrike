#include "KeyListener.h"
#include <SPI.h>
#include <math.h>

// Calculated constants
const int ENV_POINTS = 12;  // Envelope window size
const int HALF_ENV = ENV_POINTS / 2;

// ==================== Constructor and Destructor ====================

KeyListener::KeyListener() 
    : calibratedFlag(false)
    , systemInitialized(false)
    , dataCounter(0)
    , lastKeypressTime(0)
    , slopeThreshold(SLOPE_THRESH)
    , amplitudeThreshold(AMP_THRESH)
    , keyDetectedCallback(nullptr)
    , calibrationEnabled(false)
    , calibDx(0.0f)
    , calibDy(0.0f)
    , calibTheta(0.0f)
{
    // Initialize all arrays to zero
    memset(rawData, 0, sizeof(rawData));
    memset(filteredData, 0, sizeof(filteredData));
    memset(calibratedData, 0, sizeof(calibratedData));
    memset(envelopeData, 0, sizeof(envelopeData));
    memset(offsetValues, 0, sizeof(offsetValues));
    memset(filterBuffer, 0, sizeof(filterBuffer));
    memset(filterIndex, 0, sizeof(filterIndex));
    memset(filterCount, 0, sizeof(filterCount));
    memset(calibBuffer, 0, sizeof(calibBuffer));
    memset(calibIndex, 0, sizeof(calibIndex));
    memset(calibCount, 0, sizeof(calibCount));
    memset(envelopeBuffer, 0, sizeof(envelopeBuffer));
    memset(envelopeIndex, 0, sizeof(envelopeIndex));
    memset(previousValues, 0, sizeof(previousValues));
    memset(recentValues, 0, sizeof(recentValues));
    memset(recentIndex, 0, sizeof(recentIndex));

    // Initialize peak states
    for (int i = 0; i < NUM_SENSORS; i++) {
        peakStates[i] = {false, 0, 0, 0.0f, 0.0f, {{0}}};
    }
}

KeyListener::~KeyListener() {
    // Clean up resources
}

// ==================== Core Methods ====================

bool KeyListener::begin() {
    if (!initializeSensors()) {
        return false;
    }
    
    systemInitialized = true;
    return true;
}

void KeyListener::update() {
    if (!systemInitialized) {
        return;
    }

    // Trigger all sensors for simultaneous measurement
    for(int i = 0; i < NUM_SENSORS; i++) {
        sensors[i].startSingleMeasurement();
    }
    delayMicroseconds(mlx90393_tconv[2][0]*1000 + 200);

    // Read measurement results from all sensors
    readAllSensors();

    // Step 1: Apply moving average filtering
    for (int i = 0; i < NUM_SENSORS; i++) {
        for (int ax = 0; ax < 3; ax++) {
            // Update filter circular buffer
            filterBuffer[i][ax][filterIndex[i][ax]] = rawData[i][ax];
            filterIndex[i][ax] = (filterIndex[i][ax] + 1) % FILTER_WINDOW_SIZE;
            if (filterCount[i][ax] < FILTER_WINDOW_SIZE) {
                filterCount[i][ax]++;
            }
            
            // Apply moving average filtering
            filteredData[i][ax] = applyMovingAverage(i, ax);
        }
    }

    // Step 2: Calibration phase with stability detection
    if (!calibratedFlag) {
        updateCalibration();
        return;
    }

    // Step 3: Apply offset correction, envelope calculation and peak detection
    calculateEnvelope();
    detectPeaks();

    dataCounter++;
}

void KeyListener::reset() {
    calibratedFlag = false;
    dataCounter = 0;
    lastKeypressTime = 0;
    
    // Reset all buffers
    memset(filterIndex, 0, sizeof(filterIndex));
    memset(filterCount, 0, sizeof(filterCount));
    memset(calibIndex, 0, sizeof(calibIndex));
    memset(calibCount, 0, sizeof(calibCount));
    memset(envelopeIndex, 0, sizeof(envelopeIndex));
    memset(offsetValues, 0, sizeof(offsetValues));
    
    // Reset peak states
    for (int i = 0; i < NUM_SENSORS; i++) {
        peakStates[i] = {false, 0, 0, 0.0f, 0.0f, {{0}}};
    }
}

// ==================== Status Queries ====================

bool KeyListener::isCalibrated() const {
    return calibratedFlag;
}

bool KeyListener::isKeyDetected() const {
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (peakStates[i].inPeak) {
            return true;
        }
    }
    return false;
}

// ==================== Callback Function Setup ====================

void KeyListener::setKeyDetectedCallback(KeyDetectedCallback callback) {
    keyDetectedCallback = callback;
}

// ==================== Configuration Methods ====================

void KeyListener::setSampleRate(int rate) {
    // Note: Changing sample rate requires recalculation of related constants
}

void KeyListener::setThresholds(float slopeThresh, float ampThresh) {
    slopeThreshold = slopeThresh;
    amplitudeThreshold = ampThresh;
}

// ==================== Data Access Methods ====================

void KeyListener::getCurrentSensorData(SensorData data[NUM_SENSORS]) {
    for (int i = 0; i < NUM_SENSORS; i++) {
        data[i].x = envelopeData[i][0];
        data[i].y = envelopeData[i][1];
        data[i].z = envelopeData[i][2];
        data[i].magnitude = sqrt(
            envelopeData[i][0]*envelopeData[i][0] +
            envelopeData[i][1]*envelopeData[i][1] +
            envelopeData[i][2]*envelopeData[i][2]
        );
    }
}

void KeyListener::getFilteredData(float data[NUM_SENSORS][3]) {
    memcpy(data, filteredData, sizeof(filteredData));
}

void KeyListener::getOffsetValues(float offsets[NUM_SENSORS][3]) {
    memcpy(offsets, offsetValues, sizeof(offsetValues));
}

// ==================== Private Methods - Signal Processing ====================

bool KeyListener::initializeSensors() {
    for (int i = 0; i < NUM_SENSORS; i++) {
        sensors[i] = Adafruit_MLX90393();
        while (!sensors[i].begin_SPI(CS_PINS[i])) {
            delay(500);
        }
        sensors[i].setOversampling(MLX90393_OSR_0);
        sensors[i].setFilter(MLX90393_FILTER_2);
    }
    
    return true;
}

void KeyListener::readAllSensors() {
    for(int i = 0; i < NUM_SENSORS; i++) {
        if (!sensors[i].readMeasurement(&rawData[i][0], &rawData[i][1], &rawData[i][2])) {
            rawData[i][0] = rawData[i][1] = rawData[i][2] = 0.0f;
        }
    }
}

float KeyListener::applyMovingAverage(int sensorId, int axis) {
    int idx = filterIndex[sensorId][axis];
    int count = filterCount[sensorId][axis];
    
    if (count >= FILTER_WINDOW_SIZE) {
        // Calculate moving average over complete window
        float sum = 0;
        for (int i = 0; i < FILTER_WINDOW_SIZE; i++) {
            sum += filterBuffer[sensorId][axis][i];
        }
        return sum / FILTER_WINDOW_SIZE;
    } else {
        // Insufficient data, return latest value
        return filterBuffer[sensorId][axis][(idx - 1 + FILTER_WINDOW_SIZE) % FILTER_WINDOW_SIZE];
    }
}

void KeyListener::updateCalibration() {
    bool allCalibrated = true;
    
    for (int i = 0; i < NUM_SENSORS; i++) {
        // Fill calibration circular buffer
        for (int ax = 0; ax < 3; ax++) {
            calibBuffer[i][ax][calibIndex[i][ax]] = filteredData[i][ax];
            calibIndex[i][ax] = (calibIndex[i][ax] + 1) % CALIBRATION_WINDOW;
        }
        if (calibCount[i] < CALIBRATION_WINDOW) {
            calibCount[i]++;
        }
        
        // Check if calibration buffer is full
        if (calibCount[i] < CALIBRATION_WINDOW) {
            allCalibrated = false;
            continue;
        }
        
        // Check data stability (standard deviation < threshold)
        bool stable = isCalibrationStable(i);
        if (!stable) {
            allCalibrated = false;
            continue;
        }
        
        // If sensor is stable and not yet calibrated, calculate offsets
        if (offsetValues[i][0] == 0 && offsetValues[i][1] == 0 && offsetValues[i][2] == 0) {
            calculateOffsets(i);
        }
    }
    
    if (allCalibrated) {
        // Verify all sensors have calculated offsets
        bool allOffsetsCalculated = true;
        for (int i = 0; i < NUM_SENSORS; i++) {
            if (offsetValues[i][0] == 0 && offsetValues[i][1] == 0 && offsetValues[i][2] == 0) {
                allOffsetsCalculated = false;
                break;
            }
        }
        
        if (allOffsetsCalculated) {
            calibratedFlag = true;
            Serial.println("=== Calibration Complete ===");
            printBaseOffsets();
            Serial.println("Starting key detection...");
        }
    }
}

void KeyListener::calculateOffsets(int sensorId) {
    for (int ax = 0; ax < 3; ax++) {
        float sum = 0;
        for (int k = 0; k < CALIBRATION_WINDOW; k++) {
            sum += calibBuffer[sensorId][ax][k];
        }
        offsetValues[sensorId][ax] = sum / CALIBRATION_WINDOW;
    }
}

float KeyListener::computeStandardDeviation(float *buffer, int size) {
    float sum = 0, sumsq = 0;
    for (int i = 0; i < size; i++) {
        sum += buffer[i];
        sumsq += buffer[i] * buffer[i];
    }
    float mean = sum / size;
    float var = sumsq / size - mean * mean;
    return sqrt(var);
}

bool KeyListener::isCalibrationStable(int sensorId) {
    for (int ax = 0; ax < 3; ax++) {
        if (calibCount[sensorId] < CALIBRATION_WINDOW) {
            return false;
        }
        if (computeStandardDeviation(calibBuffer[sensorId][ax], CALIBRATION_WINDOW) > STD_THRESHOLD) {
            return false;
        }
    }
    return true;
}

void KeyListener::printBaseOffsets() {
    Serial.println("Base Offsets:");
    for (int i = 0; i < NUM_SENSORS; i++) {
        Serial.print("Sensor"); Serial.print(i+1); Serial.print(": ");
        Serial.print("X="); Serial.print(offsetValues[i][0], 3); Serial.print(" ");
        Serial.print("Y="); Serial.print(offsetValues[i][1], 3); Serial.print(" ");
        Serial.print("Z="); Serial.print(offsetValues[i][2], 3);
        Serial.println();
    }
}

// ==================== Private Methods - Envelope and Peak Detection ====================

void KeyListener::calculateEnvelope() {
    for(int i = 0; i < NUM_SENSORS; i++) {
        // Apply offset correction to filtered data
        for(int ax = 0; ax < 3; ax++) {
            calibratedData[i][ax] = filteredData[i][ax] - offsetValues[i][ax];

            // Update envelope calculation circular buffer
            envelopeBuffer[i][ax][envelopeIndex[i][ax]] = calibratedData[i][ax];
            envelopeIndex[i][ax] = (envelopeIndex[i][ax] + 1) % 12;

            // Calculate envelope (find original value with maximum absolute value)
            float max_abs_value = 0;
            float envelope_value = 0;
            for(int k = 0; k < 12; k++) {
                float abs_val = fabs(envelopeBuffer[i][ax][k]);
                if (abs_val > max_abs_value) {
                    max_abs_value = abs_val;
                    envelope_value = envelopeBuffer[i][ax][k];  // Preserve original sign
                }
            }
            envelopeData[i][ax] = envelope_value;  // Store envelope data
        }
    }
}

void KeyListener::detectPeaks() {
    for(int i = 0; i < NUM_SENSORS; i++) {
        // Calculate total magnetic field magnitude (vector norm)
        float totalField = sqrt(
            envelopeData[i][0]*envelopeData[i][0] +
            envelopeData[i][1]*envelopeData[i][1] +
            envelopeData[i][2]*envelopeData[i][2]
        );

        // Update dynamic baseline
        updateDynamicBaseline(i, totalField);

        // Peak detection algorithm
        float slope = totalField - previousValues[i];  // Calculate slope (first derivative)
        previousValues[i] = totalField;
        
        processPeakDetection(i, totalField, slope);
    }
}

void KeyListener::updateDynamicBaseline(int sensorId, float totalField) {
    // Update dynamic baseline level (approximate 10th percentile)
    recentValues[sensorId][recentIndex[sensorId]] = totalField;
    recentIndex[sensorId] = (recentIndex[sensorId] + 1) % 50;
    
    float base = recentValues[sensorId][0];
    for(int k = 1; k < 50; k++) {
        if (recentValues[sensorId][k] < base) base = recentValues[sensorId][k];
    }
    peakStates[sensorId].baseLevel = base;
}

void KeyListener::processPeakDetection(int sensorId, float totalField, float slope) {
    bool isRise = (slope > slopeThreshold);                              // Rising slope detected
    bool isFall = (slope < -slopeThreshold);                             // Falling slope detected
    bool above  = (totalField > peakStates[sensorId].baseLevel + amplitudeThreshold);   // Above amplitude threshold

    if (!peakStates[sensorId].inPeak) {
        // Not in peak: detect peak start condition
        if (isRise && above) {
            peakStates[sensorId].risingCnt++;
            if (peakStates[sensorId].risingCnt >= MIN_COUNT) {
                // Consecutive rising samples above threshold confirm peak start
                peakStates[sensorId].inPeak = true;
                peakStates[sensorId].maxVal = totalField;
                peakStates[sensorId].risingCnt = 0;  // Reset counter
                
                // Store current sensor data as initial peak maximum
                for (int j = 0; j < NUM_SENSORS; j++) {
                    for (int ax = 0; ax < 3; ax++) {
                        peakStates[sensorId].maxData[j][ax] = envelopeData[j][ax];
                    }
                }
            }
        } else {
            peakStates[sensorId].risingCnt = 0;  // Reset rising counter
        }
    } else {
        // In peak: track maximum value and detect peak end
        if (totalField > peakStates[sensorId].maxVal) {
            peakStates[sensorId].maxVal = totalField;
            // Update sensor data at new peak maximum
            for (int j = 0; j < NUM_SENSORS; j++) {
                for (int ax = 0; ax < 3; ax++) {
                    peakStates[sensorId].maxData[j][ax] = envelopeData[j][ax];
                }
            }
        }
        
        if (isFall) {
            peakStates[sensorId].fallingCnt++;
            if (peakStates[sensorId].fallingCnt >= MIN_COUNT && !above) {
                // Consecutive falling samples below threshold confirm peak end
                // *** Key modification: Add deduplication check ***
                if (canTriggerKeypress()) {
                    predictKeypress(peakStates[sensorId].maxData);
                    lastKeypressTime = millis();  // Update last trigger time
                }
                
                peakStates[sensorId].inPeak = false;
                peakStates[sensorId].fallingCnt = 0;
            }
        } else {
            peakStates[sensorId].fallingCnt = 0;  // Reset falling counter
        }
    }
}

// ==================== Private Methods - Machine Learning ====================

bool KeyListener::canTriggerKeypress() {
    unsigned long currentTime = millis();
    return (currentTime - lastKeypressTime) > KEYPRESS_COOLDOWN;
}

void KeyListener::predictKeypress(float peakData[8][3]) {
    // Reshape 8x3 peak data into 24-element feature array
    float input[24];
    int idx = 0;
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 3; j++) {
            input[idx++] = peakData[i][j];
        }
    }
    
    // Execute neural network inference
    int predicted_label;
    float confidence;
    
    predict_keypress(input, &predicted_label, &confidence);
    
    const char* keyName = "Unknown";
    if (predicted_label >= 0 && predicted_label < 49) {
        keyName = LABEL_NAMES[predicted_label];
    }
    
    // If calibration mode is enabled, perform coordinate transformation
    if (calibrationEnabled) {
        float detected_x, detected_y;
        if (getKeyCoordinates(keyName, detected_x, detected_y)) {
            // Apply coordinate transformation
            float transformed_x, transformed_y;
            transformDetectedKeyCoordinate(detected_x, detected_y, transformed_x, transformed_y);
            
            // Find the closest key
            float distance;
            const char* actualKey = findNearestKey(transformed_x, transformed_y, distance);
            
            // Check if distance exceeds threshold (19.05/2 = 9.525mm)
            const float DISTANCE_THRESHOLD = 9.525f;
            
            // Display detailed calibration information
            Serial.print("Detected key: ");
            Serial.print(keyName);
            Serial.print(" (");
            Serial.print(detected_x, 1);
            Serial.print(", ");
            Serial.print(detected_y, 1);
            Serial.print(") -> After calibration: ");
            
            if (distance > DISTANCE_THRESHOLD) {
                Serial.print("out of range");
                Serial.print(" (distance: ");
                Serial.print(distance, 1);
                Serial.print("mm)");
            } else {
                Serial.print(actualKey);
                Serial.print(" (");
                Serial.print(transformed_x, 1);
                Serial.print(", ");
                Serial.print(transformed_y, 1);
                Serial.print(", distance: ");
                Serial.print(distance, 1);
                Serial.print("mm)");
            }
            
            Serial.print(" Confidence: ");
            Serial.println(confidence, 4);
            
            // Call the callback function with the calibrated key
            if (keyDetectedCallback != nullptr) {
                keyDetectedCallback(actualKey, confidence);
            }
        } else {
            // Cannot find coordinates, use original key
            Serial.print("Unknown key: ");
            Serial.print(keyName);
            Serial.print(" (");
            Serial.print(confidence, 4);
            Serial.println(")");
            
            if (keyDetectedCallback != nullptr) {
                keyDetectedCallback(keyName, confidence);
            }
        }
    } else {
        // Normal mode, directly call callback function
        if (keyDetectedCallback != nullptr) {
            keyDetectedCallback(keyName, confidence);
        }
    }
}

// ==================== Calibration Related Method Implementations ====================

void KeyListener::setCalibrationParams(float dx, float dy, float theta) {
    calibDx = dx;
    calibDy = dy;
    calibTheta = theta;
}

void KeyListener::enableCalibration(bool enable) {
    calibrationEnabled = enable;
}

bool KeyListener::isCalibrationEnabled() const {
    return calibrationEnabled;
}

// Get key coordinates (copied from Calibrator)
bool KeyListener::getKeyCoordinates(const char* key, float& x, float& y) const {
    static const struct {
        const char* key;
        float x;
        float y;
    } KEYBOARD_COORDINATES[] = {
        {"Esc", 0, 0},
        {"1", 19.125, 0},
        {"2", 38.175, 0},
        {"3", 57.225, 0},
        {"4", 76.275, 0},
        {"5", 95.325, 0},
        {"6", 114.375, 0},
        {"7", 133.425, 0},
        {"8", 152.475, 0},
        {"9", 171.525, 0},
        {"0", 190.575, 0},
        {"-", 209.625, 0},
        {"Backspace", 257.305, 0},
        {"Q", 28.645, -19.55},
        {"W", 47.695, -19.55},
        {"E", 66.745, -19.55},
        {"R", 85.795, -19.55},
        {"T", 104.845, -19.55},
        {"Y", 123.895, -19.55},
        {"U", 142.945, -19.55},
        {"I", 161.995, -19.55},
        {"O", 181.045, -19.55},
        {"P", 200.095, -19.55},
        {"CapsLock", 7.15, -38.6},
        {"A", 33.405, -38.6},
        {"S", 52.455, -38.6},
        {"D", 71.505, -38.6},
        {"F", 90.555, -38.6},
        {"G", 109.605, -38.6},
        {"H", 128.655, -38.6},
        {"J", 147.705, -38.6},
        {"K", 166.755, -38.6},
        {"L", 185.805, -38.6},
        {";", 204.855, -38.6},
        {"'", 223.905, -38.6},
        {"Enter", 254.925, -38.6},
        {"Shift", 11.915, -57.65},
        {"Z", 42.935, -57.65},
        {"X", 61.985, -57.65},
        {"C", 81.035, -57.65},
        {"V", 100.085, -57.65},
        {"B", 119.135, -57.65},
        {"N", 138.185, -57.65},
        {"M", 157.235, -57.65},
        {",", 176.285, -57.65},
        {".", 195.335, -57.65},
        {"/", 214.385, -57.65},
        {"Ctrl", 1.19, -77.08},
        {"OS", 26.265, -77.08},
        {"Alt", 50.075, -77.08},
        {"Space", 121.515, -77.08}
    };
    
    static const int KEYBOARD_COORDINATES_COUNT = sizeof(KEYBOARD_COORDINATES) / sizeof(KEYBOARD_COORDINATES[0]);
    
    for (int i = 0; i < KEYBOARD_COORDINATES_COUNT; i++) {
        if (strcmp(KEYBOARD_COORDINATES[i].key, key) == 0) {
            x = KEYBOARD_COORDINATES[i].x;
            y = KEYBOARD_COORDINATES[i].y;
            return true;
        }
    }
    return false;
}

// Find the nearest key based on coordinates
const char* KeyListener::findNearestKey(float target_x, float target_y, float& minDistance) const {
    const char* allKeys[] = {
        "Esc", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "Backspace",
        "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P",
        "CapsLock", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "Enter",
        "Shift", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/",
        "Ctrl", "OS", "Alt", "Space"
    };
    
    static const int KEY_COUNT = sizeof(allKeys) / sizeof(allKeys[0]);
    
    const char* nearestKey = allKeys[0];
    minDistance = 99999.0f;
    
    for (int i = 0; i < KEY_COUNT; i++) {
        float key_x, key_y;
        if (getKeyCoordinates(allKeys[i], key_x, key_y)) {
            float distance = sqrt((target_x - key_x) * (target_x - key_x) + 
                                (target_y - key_y) * (target_y - key_y));
            if (distance < minDistance) {
                minDistance = distance;
                nearestKey = allKeys[i];
            }
        }
    }
    
    return nearestKey;
}

// Figure 2 formula: Inverse coordinate transformation for calibrated listener mode R^(-1)(θ) · (pos_eavesdrop - [dx, dy]^T)
void KeyListener::transformDetectedKeyCoordinate(float input_x, float input_y, float& output_x, float& output_y) const {
    // First subtract displacement
    float temp_x = input_x - calibDx;
    float temp_y = input_y - calibDy;
    
    // Then apply inverse rotation R^(-1)(θ) = R(-θ)
    float cos_theta = cos(-calibTheta);  // cos(-θ) = cos(θ)
    float sin_theta = sin(-calibTheta);  // sin(-θ) = -sin(θ)
    
    output_x = cos_theta * temp_x - sin_theta * temp_y;
    output_y = sin_theta * temp_x + cos_theta * temp_y;
} 