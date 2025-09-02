#ifndef TYPES_H
#define TYPES_H

// Operation mode enumeration
enum class OperationMode {
  LISTENER = 0,                   // Listener mode
  ATTACKER = 1,                   // Attacker mode
  CALIBRATION = 2,                // Calibration mode
  LISTENER_AFTER_CALIBRATION = 3, // Listener mode after calibration
  ATTACKER_AFTER_CALIBRATION = 4,  // Attacker mode after calibration
  END_TO_END = 5                   // End-to-end mode: calibration, eavesdrop, then attack if idle
};

// Sensor data structure
struct SensorData {
  float x, y, z;       // Magnetic field components
  float magnitude;     // Field magnitude
};

// Peak detection state structure
struct PeakState {
  bool  inPeak;         // Whether currently in peak detection state
  int   risingCnt;      // Consecutive rising slope count
  int   fallingCnt;     // Consecutive falling slope count
  float baseLevel;      // Dynamic baseline level
  float maxVal;         // Maximum value during current peak
  float maxData[8][3];  // Sensor data at peak maximum moment
};

// Key detection event callback function type
typedef void (*KeyDetectedCallback)(const char* keyName, float confidence);

#endif // TYPES_H 