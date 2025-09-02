#include "KeyAttacker.h"
#include <math.h>

// Shifted key mapping array (ported from main2.cpp)  
struct ShiftedKeyMapping {
    const char* shiftedChar;
    const char* baseKey;
};

static const ShiftedKeyMapping SHIFTED_KEYS[] = {
    {"!", "1"}, {"@", "2"}, {"#", "3"}, {"$", "4"}, {"%", "5"}, {"^", "6"},
    {"&", "7"}, {"*", "8"}, {"(", "9"}, {")", "0"}, {"_", "-"}, 
    {":", ";"}, {"\"", "'"}, {"<", ","}, {">", "."}, {"?", "/"},
    {"A", "a"}, {"B", "b"}, {"C", "c"}, {"D", "d"}, {"E", "e"}, {"F", "f"},
    {"G", "g"}, {"H", "h"}, {"I", "i"}, {"J", "j"}, {"K", "k"}, {"L", "l"},
    {"M", "m"}, {"N", "n"}, {"O", "o"}, {"P", "p"}, {"Q", "q"}, {"R", "r"},
    {"S", "s"}, {"T", "t"}, {"U", "u"}, {"V", "v"}, {"W", "w"}, {"X", "x"},
    {"Y", "y"}, {"Z", "z"}
  };

static const int SHIFTED_KEY_COUNT = sizeof(SHIFTED_KEYS) / sizeof(ShiftedKeyMapping);

// ==================== Constructor and Destructor ====================

KeyAttacker::KeyAttacker() 
    : mcp1(MCP23017_ADDR1)
    , mcp2(MCP23017_ADDR2)
    , mcp3(MCP23017_ADDR3)
    , systemInitialized(false)
    , currentKeyboardType("Wooting 60 HE")
    , currentCycle(375)
    , risingWarmup(::rising_warmup)
    , risingInterval(::rising_interval)
    , risingHigh(0)
    , baseRisingHigh(0)
    , currentToff(::Toff)
    , defaultToff(::defaultToff)
    , repeatedOrShiftToff(::repeatedOrShiftToff)
    , calibrationEnabled(false)
    , calibDx(0.0f)
    , calibDy(0.0f)
    , calibTheta(0.0f)
{
    initializeKeyMaps();
}

KeyAttacker::~KeyAttacker() {
    // Clean up resources
}

// ==================== Core Methods ====================

bool KeyAttacker::begin() {
    return begin("Wooting 60 HE");
}

bool KeyAttacker::begin(const char* keyboardType) {
    Wire.begin();
    
    // Configure keyboard type
    configureKeyboardType(keyboardType);
    baseRisingHigh = 2 * currentCycle;
    risingHigh = baseRisingHigh;
    
    // Initialize MCP23017 chips
    if (!initializeMCPs()) {
        return false;
    }
    
    // Initialize GPIO pins
    initializeGPIOs();
    
    systemInitialized = true;
    delay(3000);  // Startup delay
    return true;
}

void KeyAttacker::reset() {
    systemInitialized = false;
    lastAttackedKey = "";
    lastAttackedSequence = "";
    
    // Reset all MCP outputs to low level
    if (systemInitialized) {
        mcp1.writePort(MCP23017Port::A, 0x00);
        mcp1.writePort(MCP23017Port::B, 0x00);
        mcp2.writePort(MCP23017Port::A, 0x00);
        mcp2.writePort(MCP23017Port::B, 0x00);
        mcp3.writePort(MCP23017Port::A, 0x00);
        mcp3.writePort(MCP23017Port::B, 0x00);
        
        // Reset GPIO pins
        for (int i = 0; i < 3; i++) {
            digitalWrite(GPIO_PINS[i], LOW);
        }
        digitalWrite(CONTROL_PIN_17, LOW);
        digitalWrite(CONTROL_PIN_19, LOW);
    }
}

// ==================== Attack Methods ====================

void KeyAttacker::attackKey(const char* keyName) {
    attackKey(keyName, "");
}

void KeyAttacker::attackKey(const char* keyName, const char* nextKey) {
    if (!systemInitialized) {
        return;
    }
    
    const char* targetKeyName = keyName;
    
    // Coordinate transformation in calibration mode
    if (calibrationEnabled) {
        float key_x, key_y;
        if (getKeyCoordinates(keyName, key_x, key_y)) {
            // Apply coordinate transformation
            float transformed_x, transformed_y;
            transformAttackKeyCoordinate(key_x, key_y, transformed_x, transformed_y);
            
            // Find the nearest actual key
            float distance;
            const char* actualKey = findNearestKey(transformed_x, transformed_y, distance);
            
            const float DISTANCE_THRESHOLD = 9.525f;  // 19.05/2
            if (distance > DISTANCE_THRESHOLD) {
                Serial.print("Key ");
                Serial.print(keyName);
                Serial.print(" -> out of range (distance: ");
                Serial.print(distance, 1);
                Serial.println("mm), skipping attack");
                return;
            } else {
                Serial.print("Calibrated attack: ");
                Serial.print(keyName);
                Serial.print(" -> ");
                Serial.print(actualKey);
                Serial.print(" (distance: ");
                Serial.print(distance, 1);
                Serial.println("mm)");
                targetKeyName = actualKey;
            }
        }
    }
    
    const char* actualKey = targetKeyName;
    bool shiftNeeded = false;
    
    // Check if it's a shifted key
    for (int i = 0; i < SHIFTED_KEY_COUNT; i++) {
        if (strcmp(SHIFTED_KEYS[i].shiftedChar, targetKeyName) == 0) {
            actualKey = SHIFTED_KEYS[i].baseKey;
            shiftNeeded = true;
            break;
        }
    }
    
    // Set Toff based on shift needed or repeated key
    if (shiftNeeded || strcmp(keyName, nextKey) == 0) {
        currentToff = repeatedOrShiftToff;
    } else {
        currentToff = defaultToff;
    }
    
    // // Set rising_high based on key type
    // if (strcmp(keyName, "s") == 0 || strcmp(keyName, "q") == 0 || strcmp(keyName, "t") == 0) {
    //     risingHigh = baseRisingHigh + 1500;
    // } else {
    //     risingHigh = baseRisingHigh;
    // }
    
    // Search for key location and execute attack
    for (int i = 0; i < 16; i++) {
        if (strcmp(keyMap1[i], actualKey) == 0) {
            shiftNeeded ? pulsePinWithGPIOShift(mcp1, i) : pulsePin(mcp1, i);
            lastAttackedKey = String(keyName);
            return;
        }
        if (strcmp(keyMap2[i], actualKey) == 0) {
            shiftNeeded ? pulsePinWithGPIOShift(mcp2, i) : pulsePin(mcp2, i);
            lastAttackedKey = String(keyName);
            return;
        }
        if (strcmp(keyMap3[i], actualKey) == 0) {
            shiftNeeded ? pulsePinWithGPIOShift(mcp3, i) : pulsePin(mcp3, i);
            lastAttackedKey = String(keyName);
            return;
        }
    }
    
    // Check GPIO keys
    for (int i = 0; i < 3; i++) {
        if (strcmp(GPIO_KEYS[i], actualKey) == 0) {
            pulseGPIO(GPIO_PINS[i]);
            lastAttackedKey = String(keyName);
            return;
        }
    }
}

// Removed string attack methods, strictly following original implementation

// ==================== Configuration Methods ====================

void KeyAttacker::setKeyboardType(const char* type) {
    configureKeyboardType(type);
    baseRisingHigh = 2 * currentCycle;
    risingHigh = baseRisingHigh;
}

void KeyAttacker::setTimingParameters(uint warmup, uint interval, uint high, uint toff) {
    risingWarmup = warmup;
    risingInterval = interval;
    risingHigh = high;
    currentToff = toff;
}

// ==================== Status Queries ====================

bool KeyAttacker::isInitialized() const {
    return systemInitialized;
}

const char* KeyAttacker::getCurrentKeyboardType() const {
    return currentKeyboardType.c_str();
}

void KeyAttacker::getTimingParameters(uint& warmup, uint& interval, uint& high, uint& toff) {
    warmup = risingWarmup;
    interval = risingInterval;
    high = risingHigh;
    toff = currentToff;
}

// ==================== Utility Methods ====================

bool KeyAttacker::isValidKey(const char* keyName) {
    MCP23017* mcp;
    uint8_t pin;
    int gpioPin;
    
    return findKeyLocation(keyName, mcp, pin) || isGPIOKey(keyName, gpioPin);
}

void KeyAttacker::listAvailableKeys() {
    Serial.println("Available keys list:");
    Serial.println("=== MCP1 Keys ===");
    for (int i = 0; i < 16; i++) {
        Serial.print("Pin"); Serial.print(i); Serial.print(": "); Serial.println(keyMap1[i]);
    }
    
    Serial.println("=== MCP2 Keys ===");
    for (int i = 0; i < 16; i++) {
        Serial.print("Pin"); Serial.print(i); Serial.print(": "); Serial.println(keyMap2[i]);
    }
    
    Serial.println("=== MCP3 Keys ===");
    for (int i = 0; i < 16; i++) {
        Serial.print("Pin"); Serial.print(i); Serial.print(": "); Serial.println(keyMap3[i]);
    }
    
    Serial.println("=== GPIO Keys ===");
    for (int i = 0; i < 3; i++) {
        Serial.print("GPIO"); Serial.print(GPIO_PINS[i]); Serial.print(": "); Serial.println(GPIO_KEYS[i]);
    }
}

void KeyAttacker::listKeyboardTypes() {
    Serial.println("Supported keyboard types:");
    int configCount = sizeof(KEYBOARD_CONFIGS) / sizeof(KeyboardConfig);
    for (int i = 0; i < configCount; i++) {
        Serial.print(KEYBOARD_CONFIGS[i].name); Serial.print(" (cycle="); Serial.print(KEYBOARD_CONFIGS[i].cycle); Serial.println(")");
    }
}

// ==================== Advanced Features ====================



void KeyAttacker::repeatLastAttack(int times) {
    if (lastAttackedKey.length() > 0) {
        for (int i = 0; i < times; i++) {
            attackKey(lastAttackedKey.c_str());
        }
    }
}

String KeyAttacker::charToKeyName(char c) {
    if (c >= 'A' && c <= 'Z') {
        // Uppercase letters need Shift - keep them uppercase for shift detection
        return String(c);
    } else if (c >= 'a' && c <= 'z') {
        return String(c);
    } else if (c >= '0' && c <= '9') {
        return String(c);
    } else {
        // Special character mapping
        switch (c) {
            case ' ': return " ";
            case '!': return "!";
            case '@': return "@";
            case '#': return "#";
            case '$': return "$";
            case '%': return "%";
            case '^': return "^";
            case '&': return "&";
            case '*': return "*";
            case '(': return "(";
            case ')': return ")";
            case '-': return "-";
            case '_': return "_";
            case '=': return "=";
            case '+': return "+";
            case '[': return "[";
            case ']': return "]";
            case '{': return "{";
            case '}': return "}";
            case '\\': return "\\";
            case '|': return "|";
            case ';': return ";";
            case ':': return ":";
            case '\'': return "'";
            case '"': return "\"";
            case ',': return ",";
            case '<': return "<";
            case '.': return ".";
            case '>': return ">";
            case '/': return "/";
            case '?': return "?";
            case '`': return "`";
            case '~': return "~";
            default: return ""; // Unsupported character
        }
    }
}

String KeyAttacker::getCalibratedKey(const char* keyName) {
    if (!calibrationEnabled) {
        return String(keyName);  // No calibration, return original key
    }
    
    float key_x, key_y;
    if (getKeyCoordinates(keyName, key_x, key_y)) {
        // Apply coordinate transformation
        float transformed_x, transformed_y;
        transformAttackKeyCoordinate(key_x, key_y, transformed_x, transformed_y);
        
        // Find the nearest actual key
        float distance;
        const char* actualKey = findNearestKey(transformed_x, transformed_y, distance);
        
        const float DISTANCE_THRESHOLD = 9.525f;  // 19.05/2
        if (distance > DISTANCE_THRESHOLD) {
            return String("");  // Out of range
        } else {
            return String(actualKey);
        }
    }
    
    return String(keyName);  // Cannot find coordinates, return original
}

// ==================== Private Methods - Initialization ====================

bool KeyAttacker::initializeMCPs() {
    // Initialize MCP23017 chips
    mcp1.init();
    mcp2.init();
    mcp3.init();
    
    // Set all pins to output mode
    mcp1.portMode(MCP23017Port::A, 0);
    mcp1.portMode(MCP23017Port::B, 0);
    mcp2.portMode(MCP23017Port::A, 0);
    mcp2.portMode(MCP23017Port::B, 0);
    mcp3.portMode(MCP23017Port::A, 0);
    mcp3.portMode(MCP23017Port::B, 0);
    
    // Set all outputs to low level
    mcp1.writePort(MCP23017Port::A, 0x00);
    mcp1.writePort(MCP23017Port::B, 0x00);
    mcp2.writePort(MCP23017Port::A, 0x00);
    mcp2.writePort(MCP23017Port::B, 0x00);
    mcp3.writePort(MCP23017Port::A, 0x00);
    mcp3.writePort(MCP23017Port::B, 0x00);
    
    return true;
}

void KeyAttacker::initializeGPIOs() {
    // Initialize GPIO pins
    for (int i = 0; i < 3; i++) {
        pinMode(GPIO_PINS[i], OUTPUT);
        digitalWrite(GPIO_PINS[i], LOW);
    }
    
    // Initialize GPIO shift pin
    pinMode(gpioShiftPin, OUTPUT);
    digitalWrite(gpioShiftPin, LOW);
    
    // Initialize control pins
    pinMode(CONTROL_PIN_17, OUTPUT);
    digitalWrite(CONTROL_PIN_17, LOW);
    pinMode(CONTROL_PIN_19, OUTPUT);
    digitalWrite(CONTROL_PIN_19, LOW);
}

void KeyAttacker::initializeKeyMaps() {
    // Port key mappings from main2.cpp
    const char* map1[16] = {
        "4", "3", "2", "1", "Esc", "p", "o", "i",
        "Backspace", "-", "0", "9", "8", "7", "6", "5"
    };
    
    const char* map2[16] = {
        "'", ";", "l", "k", "j", "h", "g", "f",
        "u", "y", "t", "r", "e", "w", "q", "Enter"
    };
    
    const char* map3[16] = {
        "n", " ", "b", "v", "c", "x", "z", "None",
        "d", "s", "a", "CapsLk", "/", ".", ",", "m"
    };
    
    memcpy(keyMap1, map1, sizeof(map1));
    memcpy(keyMap2, map2, sizeof(map2));
    memcpy(keyMap3, map3, sizeof(map3));
}

void KeyAttacker::configureKeyboardType(const char* type) {
    currentKeyboardType = String(type);
    
    int configIndex = findKeyboardConfigIndex(type);
    if (configIndex >= 0) {
        currentCycle = KEYBOARD_CONFIGS[configIndex].cycle;
    } else {
        currentCycle = 375;  // Default value
    }
}

// ==================== Private Methods - Pulse Generation ====================

void KeyAttacker::pulsePin(MCP23017 &mcp, uint8_t pin) {
    mcp.digitalWrite(pin, HIGH);
    digitalWrite(CONTROL_PIN_17, HIGH);
    delayMicroseconds(risingWarmup);
    digitalWrite(CONTROL_PIN_19, HIGH);
    delayMicroseconds(risingInterval);
    digitalWrite(CONTROL_PIN_17, LOW);
    delayMicroseconds(risingHigh);
    digitalWrite(CONTROL_PIN_19, LOW);
    delayMicroseconds(currentToff);
    mcp.digitalWrite(pin, LOW);
}

void KeyAttacker::pulseGPIO(int pin) {
    digitalWrite(pin, HIGH);
    digitalWrite(CONTROL_PIN_17, HIGH);
    delayMicroseconds(risingWarmup);
    digitalWrite(CONTROL_PIN_19, HIGH);
    delayMicroseconds(risingInterval);
    digitalWrite(CONTROL_PIN_17, LOW);
    delayMicroseconds(risingHigh);
    digitalWrite(CONTROL_PIN_19, LOW);
    delayMicroseconds(currentToff);
    digitalWrite(pin, LOW);
}

void KeyAttacker::pulsePinWithShift(MCP23017& mcpShift, uint8_t shiftPin, 
                                    MCP23017& mcpKey, uint8_t keyPin) {
    digitalWrite(gpioShiftPin, HIGH);
    mcpKey.digitalWrite(keyPin, HIGH);
    digitalWrite(CONTROL_PIN_17, HIGH);
    delayMicroseconds(risingWarmup);
    digitalWrite(CONTROL_PIN_19, HIGH);
    delayMicroseconds(risingInterval);
    digitalWrite(CONTROL_PIN_17, LOW);
    delayMicroseconds(risingHigh);
    digitalWrite(CONTROL_PIN_19, LOW);
    delayMicroseconds(currentToff);
    digitalWrite(gpioShiftPin, LOW);
    mcpKey.digitalWrite(keyPin, LOW);
}

void KeyAttacker::pulsePinWithGPIOShift(MCP23017& mcpKey, uint8_t keyPin) {
    digitalWrite(gpioShiftPin, HIGH);
    delayMicroseconds(500);
    mcpKey.digitalWrite(keyPin, HIGH);
    digitalWrite(CONTROL_PIN_17, HIGH);
    delayMicroseconds(risingWarmup);
    digitalWrite(CONTROL_PIN_19, HIGH);
    delayMicroseconds(risingInterval);
    digitalWrite(CONTROL_PIN_17, LOW);
    delayMicroseconds(risingHigh);
    digitalWrite(CONTROL_PIN_19, LOW);
    delayMicroseconds(currentToff);
    mcpKey.digitalWrite(keyPin, LOW);
    delayMicroseconds(500);
    digitalWrite(gpioShiftPin, LOW);
}

// ==================== Private Methods - Key Processing ====================

bool KeyAttacker::findKeyLocation(const char* keyName, MCP23017*& mcp, uint8_t& pin) {
    // Search MCP1
    for (int i = 0; i < 16; i++) {
        if (strcmp(keyMap1[i], keyName) == 0) {
            mcp = &mcp1;
            pin = i;
            return true;
        }
    }
    
    // Search MCP2
    for (int i = 0; i < 16; i++) {
        if (strcmp(keyMap2[i], keyName) == 0) {
            mcp = &mcp2;
            pin = i;
            return true;
        }
    }
    
    // Search MCP3
    for (int i = 0; i < 16; i++) {
        if (strcmp(keyMap3[i], keyName) == 0) {
            mcp = &mcp3;
            pin = i;
            return true;
        }
    }
    
    return false;
}

bool KeyAttacker::isShiftedKey(const char* keyName, const char*& baseKey) {
    for (int i = 0; i < SHIFTED_KEY_COUNT; i++) {
        if (strcmp(SHIFTED_KEYS[i].shiftedChar, keyName) == 0) {
            baseKey = SHIFTED_KEYS[i].baseKey;
            return true;
        }
    }
    return false;
}

const char* KeyAttacker::getBaseKey(const char* shiftedKey) {
    for (int i = 0; i < SHIFTED_KEY_COUNT; i++) {
        if (strcmp(SHIFTED_KEYS[i].shiftedChar, shiftedKey) == 0) {
            return SHIFTED_KEYS[i].baseKey;
        }
    }
    return shiftedKey;
}

bool KeyAttacker::isGPIOKey(const char* keyName, int& pin) {
    for (int i = 0; i < 3; i++) {
        if (strcmp(GPIO_KEYS[i], keyName) == 0) {
            pin = GPIO_PINS[i];
            return true;
        }
    }
    return false;
}

// Removed string processing methods, strictly following original implementation to support only key name attacks



// ==================== Private Methods - Utility Functions ====================

int KeyAttacker::findKeyboardConfigIndex(const char* type) {
    int configCount = sizeof(KEYBOARD_CONFIGS) / sizeof(KeyboardConfig);
    for (int i = 0; i < configCount; i++) {
        if (strcmp(KEYBOARD_CONFIGS[i].name, type) == 0) {
            return i;
        }
    }
    return -1;
}



// ==================== Calibration Related Method Implementations ====================

void KeyAttacker::setCalibrationParams(float dx, float dy, float theta) {
    calibDx = dx;
    calibDy = dy;
    calibTheta = theta;
}

void KeyAttacker::enableCalibration(bool enable) {
    calibrationEnabled = enable;
}

bool KeyAttacker::isCalibrationEnabled() const {
    return calibrationEnabled;
}

// Removed calibrated string attack methods, calibration is now handled directly in attackKey

// Get key coordinates (same as in KeyListener)
bool KeyAttacker::getKeyCoordinates(const char* key, float& x, float& y) const {
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
    
    // First try direct match
    for (int i = 0; i < KEYBOARD_COORDINATES_COUNT; i++) {
        if (strcmp(KEYBOARD_COORDINATES[i].key, key) == 0) {
            x = KEYBOARD_COORDINATES[i].x;
            y = KEYBOARD_COORDINATES[i].y;
            return true;
        }
    }
    
    // If not found and it's a lowercase letter, try uppercase
    if (strlen(key) == 1 && key[0] >= 'a' && key[0] <= 'z') {
        char upperKey[2] = {(char)(key[0] - 'a' + 'A'), '\0'};
        for (int i = 0; i < KEYBOARD_COORDINATES_COUNT; i++) {
            if (strcmp(KEYBOARD_COORDINATES[i].key, upperKey) == 0) {
                x = KEYBOARD_COORDINATES[i].x;
                y = KEYBOARD_COORDINATES[i].y;
                return true;
            }
        }
    }
    
    // Handle space character
    if (strcmp(key, " ") == 0) {
        for (int i = 0; i < KEYBOARD_COORDINATES_COUNT; i++) {
            if (strcmp(KEYBOARD_COORDINATES[i].key, "Space") == 0) {
                x = KEYBOARD_COORDINATES[i].x;
                y = KEYBOARD_COORDINATES[i].y;
                return true;
            }
        }
    }
    
    return false;
}

// Find the nearest key based on coordinates
const char* KeyAttacker::findNearestKey(float target_x, float target_y, float& minDistance) const {
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

// Figure 1 formula: Coordinate transformation for calibrated attack mode R(θ) · pos_inject + [dx, dy]^T
void KeyAttacker::transformAttackKeyCoordinate(float input_x, float input_y, float& output_x, float& output_y) const {
    float cos_theta = cos(calibTheta);
    float sin_theta = sin(calibTheta);
    
    // Rotation transformation
    output_x = cos_theta * input_x - sin_theta * input_y + calibDx;
    output_y = sin_theta * input_x + cos_theta * input_y + calibDy;
}

