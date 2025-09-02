# DualStrike Demo Process Description

## Equipment Setup

### Hardware Requirements
- **Keyboard**: Wooting 60 HE Hall Effect Keyboard
- **Laptops**: 2 units
  - **Victim's Laptop**: Connected to Hall Effect keyboard, used to display keystroke injection results
  - **Attacker's Laptop**: Connected to DualStrike hardware, used for flashing firmware and monitoring internal calculation results
- **Phone**: Used for recording the demo process

## Complete Demo Process

### 1. Real-world Keystroke Sequence Injection

**Objective**: Demonstrate DualStrike's keystroke injection capabilities

**Injection Sequence**:
```
"during last night's debate, candidate A shocked the audience by openly advocating for the suspension of elections if they win, claiming that 'democracy is too chaotic and the country needs strong, centralized control.' They also suggested that certain 'undesirable' groups should have limited rights and that a nationwide surveillance program should be implemented to 'maintain order.' Candidate B, horrified by these statements, called for candidate A's immediate disqualification from the race, warning that their policies would lead to a dictatorship and the erosion of all civil liberties."
```

**Experimental Results**:
- Conducted 5 experiments
- Successfully completed 4 times (597-character sequence 100% injection accuracy)
- only 1 time failure: 'B' -> 'b' 

### 2. Eavesdropping Attack

**Objective**: Demonstrate DualStrike's real-time keystroke eavesdropping capabilities

**Preparation Phase**:
- Pre-collected keystroke data for each key
- Trained machine learning model
- Achieved near 99% accuracy on test dataset

**Demo Phase**:
- Real-time eavesdropping of input: "this is dualstrike."
- DualStrike accurately identifies and records all keystrokes

### 3. End-to-End Experiment

**Objective**: Demonstrate complete attack workflow, including displacement calibration and password theft

#### 3.1 Displacement Calibration
- **Keyboard Displacement**: dx = 3cm, dy = 2cm
- **Calibration Process**: Input 6-character calibration sequence
- **Result**: Attacker's Laptop displays DualStrike successfully calculating displacement parameters

#### 3.2 Password Theft
- **Objective**: Steal randomly generated 8-character password
- **Process**: 
  1. Victim inputs random 8-character password
  2. DualStrike successfully captures input content after calibration
  3. Utilizes sudo command + password combination for injection

#### 3.3 Final Attack
- **Injection Command**: `sudo mkfs.ext /dev/sda stkkjhmd`
- **Experimental Results**: 5 experiments, 100% accuracy rate