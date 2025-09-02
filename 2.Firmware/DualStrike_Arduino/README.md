# DualStrike Arduino Implementation

## Description

This code implements all functional features of DualStrike, running in real-time and locally on the MCU. The results can be displayed via the serial port.  
Specifically, in `main.cpp`, six operation modes are implemented:


### 1. LISTENER  
**Function:** Implements the eavesdropping feature of DualStrike (see Sec. V.A of the paper).  
When the user presses the Hall-effect keyboard, DualStrike predicts each keystroke and displays the predicted result and confidence.

**How to use:**  
- Before running, prepare the required model as described in `2.Firmware/README.md` and `3.Software/README.md`.  
- Use the Python implementation to collect data and train the MLP model for inference, which will generate an updated `keypress_model_weights.c` file.  
- Replace the existing `2.Firmware/DualStrike_Arduino/src/keypress_model_weights.c` with the new one.  
- Set `const OperationMode CURRENT_MODE = OperationMode::LISTENER;` in the code.  
- Place the Hall-effect keyboard on DualStrike and run the code. You will see inference results displayed via the serial console.

### 2. ATTACKER  
**Function:** Implements the keystroke injection feature of DualStrike (see Sec. V.B of the paper).  
The user defines a custom keystroke injection sequence in the code. With the keyboard connected to a computer, DualStrike performs injection, making the injected keystrokes visible in any open text editor.

**How to use:**  
- Set `const OperationMode CURRENT_MODE = OperationMode::ATTACKER;` in the code.  
- Specify the attack sequence in `const String ATTACK_TEXT`.  
- Supported characters include the 51 characters listed in Table II of the paper, as well as special characters such as `<>?:”—!@#$%^&*()` and uppercase letters.


### 3. CALIBRATION  
**Function:** Implements the calibration feature of DualStrike (see Sec. V.C of the paper).  
After the keyboard is displaced from its aligned position, the user enters the pre-defined calibration sequence, and DualStrike calculates and displays the resulting displacement offset.

**How to use:**  
- Set `const OperationMode CURRENT_MODE = OperationMode::CALIBRATION;` in the code.  
- Displace the keyboard as needed.  
- Enter the calibration sequence as specified in `const char* CALIBRATION_SEQUENCE` on the displaced keyboard.  
- The displacement offset will be automatically calculated and displayed.


### 4. LISTENER AFTER CALIBRATION  
**Function:** Eavesdropping after calibration.  
Once displacement and calibration are complete, pressing the keyboard prompts DualStrike to correct the eavesdropping output, and the serial console shows the actual keys being pressed.

**How to use:**  
- Set `const OperationMode CURRENT_MODE = OperationMode::LISTENER_AFTER_CALIBRATION;` in the code.  
- After completing the calibration process, the system will automatically calibrate the detected keystrokes and display the actual keys.


### 5. ATTACKER AFTER CALIBRATION  
**Function:** Keystroke injection after calibration.  
Under the same post-calibration conditions, DualStrike compensates the user-specified injection sequence so that the attack produces the intended keystrokes.

**How to use:**  
- Set `const OperationMode CURRENT_MODE = OperationMode::ATTACK_AFTER_CALIBRATION;` in the code.  
- After completing the calibration process, DualStrike will calibrate the attack sequence according to the offset, ensuring the injected keystrokes match the intended input.

### 6. END-TO-END  
**Function:** Implements the end-to-end attack as described in Sec. VI.E of the paper.  
After introducing real-world disturbance (displacement), DualStrike first performs calibration, then eavesdrops to obtain information such as credentials, and finally performs keystroke injection, achieving a complete end-to-end attack.

**How to use:**  
- Set `const OperationMode CURRENT_MODE = OperationMode::END_TO_END;` in the code.  
- First, introduce displacement to the keyboard. The code will automatically perform calibration, then proceed to eavesdrop (e.g., capturing input such as a password).  
- After eavesdropping, there will be a 30-second window. DualStrike will then combine the captured password with the attack text as the actual attack sequence and execute keystroke injection.
