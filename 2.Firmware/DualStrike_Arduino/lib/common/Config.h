#ifndef CONFIG_H
#define CONFIG_H

// ==================== Listener Configuration ====================
// Sensor configuration
#define NUM_SENSORS           8       // Number of MLX90393 magnetic sensors
#define SAMPLE_RATE           250     // Sampling frequency (Hz)
#define ENV_SEC               0.05f   // Envelope calculation window duration (seconds)

// Calibration parameters
#define CALIBRATION_WINDOW    50      // Calibration buffer window size (50 samples)
#define STD_THRESHOLD         8.0f    // Standard deviation threshold for calibration stability

// Moving average filter parameters
#define FILTER_WINDOW_SIZE    15      // Moving average filter window size (15 samples)

// Peak detection parameters
#define SLOPE_THRESH          0.16f   // Slope threshold for peak detection
#define AMP_THRESH            5.0f    // Amplitude threshold above baseline
#define MIN_COUNT             3       // Minimum consecutive samples to confirm peak start/end

// Sensor hardware configuration
static const int CS_PINS[NUM_SENSORS] = {5, 7, 9, 10, 11, 15, 27, 28};  // SPI chip select pins

// Key label mapping for machine learning model output
static const char* LABEL_NAMES[] = {
  "'", ",", "-", ".", "/", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ";", "A", "Alt", "B", "C", "CapsLock", 
  "Ctrl", "D", "E", "Esc", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "OS", "P", "Q", "R", "S", "Shift", 
  "Space", "T", "U", "V", "W", "X", "Y", "Z"
};

// ==================== Attacker Configuration ====================
// MCP23017 I2C addresses
#define MCP23017_ADDR1 0x20
#define MCP23017_ADDR2 0x21
#define MCP23017_ADDR3 0x22

// Pulse timing parameters (in microseconds)
static uint rising_warmup = 1900;      // Warmup time before pulse
static uint rising_interval = 300;     // Interval between control signals
static uint Toff = 0;                  // Current off-time
static uint defaultToff = 0;           // Default off-time
static uint repeatedOrShiftToff = 1000; // Off-time for repeated or shifted keys

// GPIO-controlled keys
static const char* GPIO_KEYS[3] = {"alt", "os", "ctrl"};
static const int GPIO_PINS[3] = {2, 3, 4};

// Control pins for pulse generation
#define CONTROL_PIN_17 17
#define CONTROL_PIN_19 19

// Keyboard type cycle configuration
struct KeyboardConfig {
  const char* name;  // Keyboard name
  int cycle;         // Cycle timing value
};

static const KeyboardConfig KEYBOARD_CONFIGS[] = {
  {"Wooting 60 HE", 375},
  {"Steelseries Pro", 990},
  {"Keydous NJ98-CP", 5160},
  {"k70 pro max", 900},
  {"Reddragon M61", 1000},
  {"DrunkDeer A75pro", 880}
};

// ==================== General Configuration ====================
// Serial communication configuration
#define SERIAL_BAUD_RATE      115200

// Debug configuration
#define DEBUG_ENABLED         true
#define DEBUG_PEAK_DATA       true
#define DEBUG_INFERENCE_TIME  true

#endif // CONFIG_H 