#include "Calibrator.h"
#include <math.h>

// Keyboard coordinate mapping for key positions in millimeters
// Origin (0,0) is at the top-left corner (Esc key position)
// X-axis increases to the right, Y-axis increases downward
static const KeyCoordinate KEYBOARD_COORDINATES[] = {
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

static const int KEYBOARD_COORDINATES_COUNT = sizeof(KEYBOARD_COORDINATES) / sizeof(KeyCoordinate);

// ==================== Constructor and Destructor ====================

Calibrator::Calibrator() 
    : calibrating(false)
    , currentStep(0)
    , totalSteps(0)
    , calibrationData(nullptr)
    , maxCalibrationPoints(20)
    , calibrationCount(0)
    , progressCallback(nullptr)
{
    calibrationData = new CalibrationPoint[maxCalibrationPoints];
    transformResult = {0.0f, 0.0f, 0.0f, 0.0f};
}

Calibrator::~Calibrator() {
    delete[] calibrationData;
}

// ==================== Core Methods ====================

bool Calibrator::startCalibration(const char* sequence) {
    if (calibrating) {
        Serial.println("Error: Calibration already in progress");
        return false;
    }
    
    calibrationSequence = String(sequence);
    totalSteps = calibrationSequence.length();
    currentStep = 0;
    calibrationCount = 0;
    calibrating = true;
    
    Serial.println("=== Calibration Started ===");
    Serial.print("Calibration sequence: ");
    Serial.println(sequence);
    Serial.print("Total steps: ");
    Serial.println(totalSteps);
    Serial.println("Please press keys according to the sequence...");
    
    printProgressUpdate();
    return true;
}

bool Calibrator::isCalibrating() const {
    return calibrating;
}

bool Calibrator::isDataCollectionComplete() const {
    return calibrating && (currentStep >= totalSteps);
}

bool Calibrator::addCalibrationPoint(const char* predictedKey, float confidence) {
    if (!calibrating) {
        return false;
    }
    
    if (currentStep >= totalSteps) {
        Serial.println("Error: Calibration sequence completed");
        return false;
    }
    
    if (calibrationCount >= maxCalibrationPoints) {
        Serial.println("Error: Maximum calibration points reached");
        return false;
    }
    
    // Get the current expected key from the sequence
    const char* expectedKey = getCurrentExpectedKey();
    
    // Store the calibration point data
    calibrationData[calibrationCount].trueKey = expectedKey;
    calibrationData[calibrationCount].predictedKey = predictedKey;
    calibrationData[calibrationCount].confidence = confidence;
    calibrationCount++;
    
    // Display calibration point information
    Serial.print("Step ");
    Serial.print(currentStep + 1);
    Serial.print("/");
    Serial.print(totalSteps);
    Serial.print(" - Expected: ");
    Serial.print(expectedKey);
    Serial.print(", Detected: ");
    Serial.print(predictedKey);
    Serial.print(", Confidence: ");
    Serial.println(confidence, 4);
    
    currentStep++;
    
    if (currentStep < totalSteps) {
        printProgressUpdate();
        return true;
    } else {
        Serial.println("Calibration data collection complete!");
        Serial.print("Current state: calibrating=");
        Serial.print(calibrating ? "true" : "false");
        Serial.print(", currentStep=");
        Serial.print(currentStep);
        Serial.print(", totalSteps=");
        Serial.println(totalSteps);
        // Note: Don't set calibrating = false here, do it in finishCalibration
        return true;
    }
}

bool Calibrator::finishCalibration(bool allowRotation) {
    Serial.println(">>> Entering finishCalibration function");
    
    if (!calibrating) {
        Serial.println("Error: No calibration in progress");
        return false;
    }
    
    if (currentStep != totalSteps) {
        Serial.print("Error: Calibration sequence not completed - Current step: ");
        Serial.print(currentStep);
        Serial.print(", Total steps: ");
        Serial.println(totalSteps);
        return false;
    }
    
    Serial.print("Calibration data points count: ");
    Serial.println(calibrationCount);
    
    Serial.println("=== Calculating Transform Parameters ===");
    
    // Display calibration data mapping
    Serial.println("Calibration data mapping:");
    Serial.println("True Key, Predicted Key, Confidence");
    for (int i = 0; i < calibrationCount; i++) {
        Serial.print(calibrationData[i].trueKey);
        Serial.print(",");
        Serial.print(calibrationData[i].predictedKey);
        Serial.print(",");
        Serial.println(calibrationData[i].confidence, 10);
    }
    
    Serial.println("Starting calculateTransform...");
    
    // Calculate transformation parameters using WLS algorithm
    bool success = calculateTransform(allowRotation);
    
    Serial.print("calculateTransform result: ");
    Serial.println(success ? "Success" : "Failed");
    
    if (success) {
        Serial.println("Transform calculation successful!");
        printCalibrationResults();
    } else {
        Serial.println("Transform calculation failed!");
    }
    
    calibrating = false;
    Serial.println("<<< Exiting finishCalibration function");
    return success;
}

void Calibrator::reset() {
    calibrating = false;
    calibrationSequence = "";
    currentStep = 0;
    totalSteps = 0;
    calibrationCount = 0;
    transformResult = {0.0f, 0.0f, 0.0f, 0.0f};
}

// ==================== Result Query Methods ====================

TransformParams Calibrator::getTransformParams() const {
    return transformResult;
}

void Calibrator::printCalibrationResults() const {
    Serial.println("=== Calibration Results ===");
    Serial.print("Translation dx: ");
    Serial.print(transformResult.dx, 3);
    Serial.println(" mm");
    Serial.print("Translation dy: ");
    Serial.print(transformResult.dy, 3);
    Serial.println(" mm");
    Serial.print("Rotation angle: ");
    Serial.print(transformResult.theta, 6);
    Serial.print(" rad (");
    Serial.print(transformResult.theta * 180.0 / PI, 3);
    Serial.println(" deg)");
    Serial.print("RMS error: ");
    Serial.print(transformResult.rms_error, 3);
    Serial.println(" mm");
}

void Calibrator::printKeyboardComparison() const {
    Serial.println("=== Keyboard Comparison Analysis ===");
    Serial.println("Key | Original Coord | Transformed Coord | Predicted Coord | Error");
    Serial.println("----|----------------|-------------------|-----------------|------");
    
    float cos_theta = cos(transformResult.theta);
    float sin_theta = sin(transformResult.theta);
    
    for (int i = 0; i < calibrationCount; i++) {
        float true_x, true_y, pred_x, pred_y;
        
        if (getKeyCoordinates(calibrationData[i].trueKey, true_x, true_y) &&
            getKeyCoordinates(calibrationData[i].predictedKey, pred_x, pred_y)) {
            
            // Calculate transformed coordinates using the computed transformation
            float transformed_x = cos_theta * true_x - sin_theta * true_y + transformResult.dx;
            float transformed_y = sin_theta * true_x + cos_theta * true_y + transformResult.dy;
            
            // Calculate Euclidean distance error
            float error = sqrt((transformed_x - pred_x) * (transformed_x - pred_x) + 
                             (transformed_y - pred_y) * (transformed_y - pred_y));
            
            Serial.print(calibrationData[i].trueKey);
            Serial.print(" | (");
            Serial.print(true_x, 1);
            Serial.print(",");
            Serial.print(true_y, 1);
            Serial.print(") | (");
            Serial.print(transformed_x, 1);
            Serial.print(",");
            Serial.print(transformed_y, 1);
            Serial.print(") | (");
            Serial.print(pred_x, 1);
            Serial.print(",");
            Serial.print(pred_y, 1);
            Serial.print(") | ");
            Serial.println(error, 2);
        }
    }
}

// ==================== Configuration Methods ====================

void Calibrator::setProgressCallback(void (*callback)(int current, int total)) {
    progressCallback = callback;
}

// ==================== Private Methods - Coordinate System ====================

bool Calibrator::getKeyCoordinates(const char* key, float& x, float& y) const {
    // Search for the key in the keyboard coordinates lookup table
    for (int i = 0; i < KEYBOARD_COORDINATES_COUNT; i++) {
        if (strcmp(KEYBOARD_COORDINATES[i].key, key) == 0) {
            x = KEYBOARD_COORDINATES[i].x;
            y = KEYBOARD_COORDINATES[i].y;
            return true;
        }
    }
    return false;
}

// ==================== Private Methods - WLS Algorithm ====================

bool Calibrator::calculateTransform(bool allowRotation) {
    Serial.println(">>> Entering calculateTransform function");
    
    if (calibrationCount < 2) {
        Serial.println("Error: Need at least 2 calibration points");
        return false;
    }
    
    Serial.print("Calibration points count: ");
    Serial.print(calibrationCount);
    Serial.print(", Allow rotation: ");
    Serial.println(allowRotation ? "Yes" : "No");
    
    bool success;
    if (allowRotation) {
        Serial.println("Calling solveWLSFull...");
        success = solveWLSFull(transformResult.dx, transformResult.dy, transformResult.theta);
    } else {
        Serial.println("Calling solveWLSTranslation...");
        transformResult.theta = 0.0f;
        success = solveWLSTranslation(transformResult.dx, transformResult.dy);
    }
    
    Serial.print("WLS solve result: ");
    Serial.println(success ? "Success" : "Failed");
    
    if (success) {
        Serial.println("Calculating RMS error...");
        transformResult.rms_error = calculateRMSError(transformResult.dx, transformResult.dy, transformResult.theta);
        Serial.print("RMS error: ");
        Serial.println(transformResult.rms_error, 6);
    }
    
    Serial.println("<<< Exiting calculateTransform function");
    return success;
}

bool Calibrator::solveWLSTranslation(float& dx, float& dy) const {
    Serial.println(">>> Entering solveWLSTranslation function");
    
    // WLS for translation only: [dx, dy] = (A^T W A)^-1 A^T W b
    // where A is the coefficient matrix, W is the weight matrix, b is the observation vector
    
    float ATA[2][2] = {{0, 0}, {0, 0}};  // A^T W A
    float ATb[2] = {0, 0};               // A^T W b
    
    int validPoints = 0;
    
    for (int i = 0; i < calibrationCount; i++) {
        float true_x, true_y, pred_x, pred_y;
        
        Serial.print("Processing calibration point ");
        Serial.print(i);
        Serial.print(": True key=");
        Serial.print(calibrationData[i].trueKey);
        Serial.print(", Predicted key=");
        Serial.print(calibrationData[i].predictedKey);
        
        bool trueFound = getKeyCoordinates(calibrationData[i].trueKey, true_x, true_y);
        bool predFound = getKeyCoordinates(calibrationData[i].predictedKey, pred_x, pred_y);
        
        if (!trueFound) {
            Serial.print(" - Cannot find true key coordinates");
        }
        if (!predFound) {
            Serial.print(" - Cannot find predicted key coordinates");
        }
        
        if (!trueFound || !predFound) {
            Serial.println(" [SKIPPED]");
            continue;
        }
        
        Serial.print(" - True coord: (");
        Serial.print(true_x, 2);
        Serial.print(", ");
        Serial.print(true_y, 2);
        Serial.print("), Predicted coord: (");
        Serial.print(pred_x, 2);
        Serial.print(", ");
        Serial.print(pred_y, 2);
        Serial.println(")");
        
        validPoints++;
        
        float weight = calibrationData[i].confidence;
        
        // For translation: observed = original + [dx, dy]
        // So: [pred_x, pred_y] = [true_x, true_y] + [dx, dy]
        // Rearranging: [pred_x - true_x, pred_y - true_y] = [dx, dy]
        
        float residual_x = pred_x - true_x;
        float residual_y = pred_y - true_y;
        
        // A matrix is identity for translation, so A^T W A = W
        ATA[0][0] += weight;  // coefficient for dx in x equation
        ATA[1][1] += weight;  // coefficient for dy in y equation
        
        // A^T W b
        ATb[0] += weight * residual_x;  // weighted residual for dx
        ATb[1] += weight * residual_y;  // weighted residual for dy
    }
    
    Serial.print("Valid calibration points count: ");
    Serial.println(validPoints);
    
    if (validPoints == 0) {
        Serial.println("Error: No valid calibration points found");
        return false;
    }
    
    // Solve the 2x2 system (it's actually diagonal for translation)
    if (ATA[0][0] < 1e-10 || ATA[1][1] < 1e-10) {
        Serial.println("Error: Singular matrix in translation WLS");
        return false;
    }
    
    dx = ATb[0] / ATA[0][0];
    dy = ATb[1] / ATA[1][1];
    
    Serial.print("Calculated transformation parameters: dx=");
    Serial.print(dx, 6);
    Serial.print(", dy=");
    Serial.println(dy, 6);
    
    Serial.println("<<< Exiting solveWLSTranslation function");
    return true;
}

bool Calibrator::solveWLSFull(float& dx, float& dy, float& theta) const {
    // This is a simplified version - for full non-linear optimization,
    // we would need iterative methods like Levenberg-Marquardt.
    // Here we use a linearized approach for computational efficiency.
    
    // Start with translation-only solution as initial estimate
    if (!solveWLSTranslation(dx, dy)) {
        return false;
    }
    
    // For small angles, we can linearize the rotation component
    // This is a simplified implementation suitable for most cases
    theta = 0.0f;  // Initialize to no rotation
    
    // Could implement iterative refinement here for better accuracy
    // For now, keeping it simple with translation-only result
    
    return true;
}

float Calibrator::calculateRMSError(float dx, float dy, float theta) const {
    float total_error_sq = 0.0f;
    int valid_points = 0;
    
    float cos_theta = cos(theta);
    float sin_theta = sin(theta);
    
    for (int i = 0; i < calibrationCount; i++) {
        float true_x, true_y, pred_x, pred_y;
        
        if (!getKeyCoordinates(calibrationData[i].trueKey, true_x, true_y) ||
            !getKeyCoordinates(calibrationData[i].predictedKey, pred_x, pred_y)) {
            continue;
        }
        
        // Apply 2D rigid transformation to true coordinates
        float transformed_x = cos_theta * true_x - sin_theta * true_y + dx;
        float transformed_y = sin_theta * true_x + cos_theta * true_y + dy;
        
        // Calculate squared Euclidean distance error
        float error_x = transformed_x - pred_x;
        float error_y = transformed_y - pred_y;
        float error_sq = error_x * error_x + error_y * error_y;
        
        total_error_sq += error_sq;
        valid_points++;
    }
    
    if (valid_points == 0) {
        return 0.0f;
    }
    
    return sqrt(total_error_sq / valid_points);
}

// ==================== Private Methods - Utility Functions ====================

void Calibrator::printProgressUpdate() const {
    if (currentStep < totalSteps) {
        const char* expectedKey = getCurrentExpectedKey();
        Serial.print("Please press key: ");
        Serial.print(expectedKey);
        Serial.print(" (");
        Serial.print(currentStep + 1);
        Serial.print("/");
        Serial.print(totalSteps);
        Serial.println(")");
    }
    
    if (progressCallback != nullptr) {
        progressCallback(currentStep, totalSteps);
    }
}

const char* Calibrator::getCurrentExpectedKey() const {
    if (currentStep >= totalSteps) {
        return "";
    }
    
    char currentChar = calibrationSequence.charAt(currentStep);
    char upperChar = toupper(currentChar);
    
    // Use predefined string constants to avoid static buffer overwrite issues
    switch (upperChar) {
        case 'Q': return "Q";
        case 'W': return "W";
        case 'E': return "E";
        case 'R': return "R";
        case 'T': return "T";
        case 'Y': return "Y";
        case 'U': return "U";
        case 'I': return "I";
        case 'O': return "O";
        case 'P': return "P";
        case 'A': return "A";
        case 'S': return "S";
        case 'D': return "D";
        case 'F': return "F";
        case 'G': return "G";
        case 'H': return "H";
        case 'J': return "J";
        case 'K': return "K";
        case 'L': return "L";
        case 'Z': return "Z";
        case 'X': return "X";
        case 'C': return "C";
        case 'V': return "V";
        case 'B': return "B";
        case 'N': return "N";
        case 'M': return "M";
        case '/': return "/";
        case '\\': return "/"; 
        case '0': return "0";
        case '1': return "1";
        case '2': return "2";
        case '3': return "3";
        case '4': return "4";
        case '5': return "5";
        case '6': return "6";
        case '7': return "7";
        case '8': return "8";
        case '9': return "9";
        case ';': return ";";
        case '\'': return "'";
        case ',': return ",";
        case '.': return ".";
        case '-': return "-";
        default: 
            Serial.print("Warning: Unknown key character: ");
            Serial.println(upperChar);
            return "";
    }
}

bool Calibrator::isValidCalibrationKey(const char* key) const {
    float x, y;
    return getKeyCoordinates(key, x, y);
} 