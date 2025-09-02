#ifndef KEYATTACKER_H
#define KEYATTACKER_H

#include <Wire.h>
#include <MCP23017.h>
#include <Arduino.h>
#include <Types.h>
#include <Config.h>

#define gpioShiftPin 16

class KeyAttacker {
public:
    // Constructor and destructor
    KeyAttacker();
    ~KeyAttacker();

    // Core methods
    bool begin();                                    // Initialize MCP23017 and GPIO systems
    bool begin(const char* keyboardType);           // Initialize with specified keyboard type
    void reset();                                    // Reset system state and outputs
    
    // Attack methods
    void attackKey(const char* keyName);             // Attack a single key by name
    void attackKey(const char* keyName, const char* nextKey);  // Attack a single key with next key context
    
    // Calibration extension methods
    void setCalibrationParams(float dx, float dy, float theta);  // Set calibration parameters
    void enableCalibration(bool enable);                        // Enable/disable calibration mode
    bool isCalibrationEnabled() const;                          // Check if calibration is enabled
    
    // Configuration methods
    void setKeyboardType(const char* type);          // Set keyboard type
    void setTimingParameters(uint warmup, uint interval, uint high, uint toff);  // Set timing parameters
    
    // Status query methods
    bool isInitialized() const;                      // Check initialization status
    const char* getCurrentKeyboardType() const;      // Get current keyboard type
    void getTimingParameters(uint& warmup, uint& interval, uint& high, uint& toff);  // Get timing parameters
    
    // Utility methods
    bool isValidKey(const char* keyName);            // Check if key name is valid
    void listAvailableKeys();                        // List all available keys
    void listKeyboardTypes();                        // List all supported keyboard types
    
    // Advanced functionality
    void repeatLastAttack(int times);                // Repeat last attack multiple times
    String charToKeyName(char c);                    // Convert character to key name
    String getCalibratedKey(const char* keyName);    // Get calibrated key target (for display)
    
private:
    // MCP23017 instances for I2C communication
    MCP23017 mcp1;
    MCP23017 mcp2;
    MCP23017 mcp3;
    
    // System state variables
    bool systemInitialized;        // Flag indicating if system is initialized
    String currentKeyboardType;    // Current keyboard type name
    int currentCycle;              // Current cycle timing value
    
    // Timing parameters for pulse generation
    uint risingWarmup;            // Warmup time in microseconds
    uint risingInterval;          // Interval time in microseconds
    uint risingHigh;              // High state duration in microseconds
    uint baseRisingHigh;          // Base high state duration in microseconds
    uint currentToff;             // Current off-time in microseconds
    uint defaultToff;             // Default off-time in microseconds
    uint repeatedOrShiftToff;     // Off-time for repeated or shifted keys
    
    // Key mapping arrays for each MCP23017
    const char* keyMap1[16];      // Key mapping for MCP1
    const char* keyMap2[16];      // Key mapping for MCP2
    const char* keyMap3[16];      // Key mapping for MCP3
    
    // Attack history tracking
    String lastAttackedKey;       // Last attacked key name
    String lastAttackedSequence;  // Last attacked sequence
    
    // Calibration transformation parameters
    bool calibrationEnabled;      // Flag indicating if calibration is enabled
    float calibDx, calibDy, calibTheta;  // Calibration parameters (translation and rotation)
    
public:
    // Structure for shifted key mappings
    struct ShiftedKeyMapping {
        const char* shiftedChar;  // The shifted character (e.g., "!")
        const char* baseKey;      // The base key (e.g., "1")
    };

private:
    
    // Private methods - Initialization
    bool initializeMCPs();                           // Initialize MCP23017 chips
    void initializeGPIOs();                          // Initialize GPIO pins
    void initializeKeyMaps();                        // Initialize key mapping arrays
    void configureKeyboardType(const char* type);    // Configure keyboard type parameters
    
    // Private methods - Calibration coordinate transformation
    bool getKeyCoordinates(const char* key, float& x, float& y) const;  // Get key coordinates
    const char* findNearestKey(float target_x, float target_y, float& minDistance) const;  // Find nearest key
    void transformAttackKeyCoordinate(float input_x, float input_y, float& output_x, float& output_y) const;  // Transform coordinates
    
    // Private methods - Pulse generation
    void pulsePin(MCP23017 &mcp, uint8_t pin);       // Generate pulse on MCP pin
    void pulseGPIO(int pin);                         // Generate pulse on GPIO pin
    void pulsePinWithShift(MCP23017& mcpShift, uint8_t shiftPin, 
                           MCP23017& mcpKey, uint8_t keyPin);  // Generate pulse with shift key
    void pulsePinWithGPIOShift(MCP23017& mcpKey, uint8_t keyPin);  // Generate pulse with GPIO shift key
    
    // Private methods - Key handling
    bool findKeyLocation(const char* keyName, MCP23017*& mcp, uint8_t& pin);  // Find key location
    bool isShiftedKey(const char* keyName, const char*& baseKey);    // Check if key requires shift
    const char* getBaseKey(const char* shiftedKey);  // Get base key for shifted character
    bool isGPIOKey(const char* keyName, int& pin);   // Check if key is on GPIO
    
    // Private methods - Utility functions
    int findKeyboardConfigIndex(const char* type);   // Find keyboard configuration index
};

#endif // KEYATTACKER_H 