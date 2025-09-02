#ifndef CALIBRATOR_H
#define CALIBRATOR_H

#include <Arduino.h>
#include <Types.h>

// Structure to store keyboard key coordinates in millimeters
struct KeyCoordinate {
    const char* key;  // Key name (e.g., "A", "Space", "Enter")
    float x;          // X coordinate in millimeters
    float y;          // Y coordinate in millimeters
};

// Structure to store calibration data points
struct CalibrationPoint {
    const char* trueKey;       // The actual key that was pressed
    const char* predictedKey;  // The key predicted by the ML model
    float confidence;          // Confidence score of the prediction
};

// Structure to store transformation parameters
struct TransformParams {
    float dx;         // Translation in X direction (millimeters)
    float dy;         // Translation in Y direction (millimeters)
    float theta;      // Rotation angle (radians)
    float rms_error;  // Root Mean Square error (millimeters)
};

constexpr bool ALLOW_ROTATION = true; // Whether to allow rotation in calibration

class Calibrator {
public:
    // Constructor and destructor
    Calibrator();
    ~Calibrator();
    
    // Core methods
    bool startCalibration(const char* sequence);     // Start calibration with specified key sequence
    bool isCalibrating() const;                      // Check if calibration is in progress
    bool isDataCollectionComplete() const;           // Check if data collection is complete
    bool addCalibrationPoint(const char* predictedKey, float confidence);  // Add calibration point
    bool finishCalibration(bool allowRotation = false);  // Complete calibration and calculate transform
    void reset();                                    // Reset calibration state
    
    // Result query methods
    TransformParams getTransformParams() const;      // Get transformation parameters
    void printCalibrationResults() const;            // Print calibration results
    void printKeyboardComparison() const;            // Print keyboard comparison analysis
    
    // Configuration methods
    void setProgressCallback(void (*callback)(int current, int total));  // Set progress callback
    
private:
    // Calibration state variables
    bool calibrating;                    // Flag indicating if calibration is in progress
    String calibrationSequence;         // The sequence of keys to be pressed
    int currentStep;                     // Current step in the calibration sequence
    int totalSteps;                      // Total number of steps in the sequence
    CalibrationPoint* calibrationData;   // Array of calibration data points
    int maxCalibrationPoints;            // Maximum number of calibration points
    int calibrationCount;                // Current number of calibration points
    
    // Transformation parameters
    TransformParams transformResult;     // Computed transformation parameters
    
    // Callback function
    void (*progressCallback)(int current, int total);  // Progress callback function
    
    // Private methods - Coordinate system
    bool getKeyCoordinates(const char* key, float& x, float& y) const;  // Get key coordinates
    void initializeKeyboardCoordinates();                               // Initialize keyboard layout
    
    // Private methods - WLS algorithm
    bool calculateTransform(bool allowRotation);                        // Calculate transformation
    float calculateRMSError(float dx, float dy, float theta) const;     // Calculate RMS error
    bool solveWLSTranslation(float& dx, float& dy) const;              // Solve translation-only WLS
    bool solveWLSFull(float& dx, float& dy, float& theta) const;       // Solve full WLS with rotation
    
    // Private methods - Mathematical operations
    bool matrixInverse2x2(float a[2][2], float inv[2][2]) const;       // 2x2 matrix inverse
    bool matrixInverse3x3(float a[3][3], float inv[3][3]) const;       // 3x3 matrix inverse
    void matrixMultiply2x2(const float a[2][2], const float b[2][2], float result[2][2]) const;  // 2x2 matrix multiply
    void matrixMultiply3x3(const float a[3][3], const float b[3][3], float result[3][3]) const;  // 3x3 matrix multiply
    
    // Private methods - Utility functions
    void printProgressUpdate() const;                                   // Print progress update
    const char* getCurrentExpectedKey() const;                          // Get current expected key
    bool isValidCalibrationKey(const char* key) const;                  // Check if key is valid for calibration
};

#endif // CALIBRATOR_H 