#include <Wire.h>
#include <MCP23017.h>

// ========== 全局参数 ==========
int cycle = 0;
String keyboardType = "Wooting 60 HE";

static uint rising_warmup = 1900;
static uint rising_interval = 200;
static uint rising_high;
static uint Toff = 0;
static uint defaultToff = 0;
static uint repeatedOrShiftToff = 10000;

// MCP 实例
#define MCP23017_ADDR1 0x20
#define MCP23017_ADDR2 0x21
#define MCP23017_ADDR3 0x22
MCP23017 mcp1(MCP23017_ADDR1);
MCP23017 mcp2(MCP23017_ADDR2);
MCP23017 mcp3(MCP23017_ADDR3);

// Key Map
const char* keyMap1[16] = {
  "4", "3", "2", "1", "Esc", "p", "o", "i",
  "Backspace", "-", "0", "9", "8", "7", "6", "5"
};

const char* keyMap2[16] = {
  "'", ";", "l", "k", "j", "h", "g", "f",
  "u", "y", "t", "r", "e", "w", "q", "Enter"
};

const char* keyMap3[16] = {
  "n", " ", "b", "v", "c", "x", "z", "shift",
  "d", "s", "a", "CapsLk", "/", ".", ",", "m"
};

// GPIO 控制的按键
const char* gpioKeys[3] = {"gpio2-alt", "gpio3-os", "gpio4-alt"};
const int gpioPins[3] = {2, 3, 4};

// Shifted key 映射
struct ShiftedKey {
  const char* shiftedChar;
  const char* baseKey;
};

ShiftedKey shiftedKeys[] = {
  {"!", "1"}, {"@", "2"}, {"#", "3"}, {"$", "4"},
  {"%", "5"}, {"^", "6"}, {"&", "7"}, {"*", "8"},
  {"(", "9"}, {")", "0"}, {"_", "-"}, {"+", "="},
  {":", ";"}, {"\"", "'"},
  {"<", ","}, {">", "."}, {"?", "/"},
  {"A", "a"}, {"B", "b"}, {"C", "c"}, {"D", "d"},
  {"E", "e"}, {"F", "f"}, {"G", "g"}, {"H", "h"},
  {"I", "i"}, {"J", "j"}, {"K", "k"}, {"L", "l"},
  {"M", "m"}, {"N", "n"}, {"O", "o"}, {"P", "p"},
  {"Q", "q"}, {"R", "r"}, {"S", "s"}, {"T", "t"},
  {"U", "u"}, {"V", "v"}, {"W", "w"}, {"X", "x"},
  {"Y", "y"}, {"Z", "z"}
};
const int shiftedKeyCount = sizeof(shiftedKeys) / sizeof(ShiftedKey);

// ========== 脉冲函数 ==========
void PulsePin(MCP23017 &mcp, uint8_t pin) {
  mcp.digitalWrite(pin, HIGH);
  digitalWrite(17, HIGH);
  delayMicroseconds(rising_warmup);
  digitalWrite(19, HIGH);
  delayMicroseconds(rising_interval);
  digitalWrite(17, LOW);
  delayMicroseconds(rising_high);
  digitalWrite(19, LOW);
  mcp.digitalWrite(pin, LOW);
  delayMicroseconds(Toff);
}

void PulseGPIO(int pin) {
  digitalWrite(pin, HIGH);
  digitalWrite(17, HIGH);
  delayMicroseconds(rising_warmup);
  digitalWrite(19, HIGH);
  delayMicroseconds(rising_interval);
  digitalWrite(17, LOW);
  delayMicroseconds(rising_high);
  digitalWrite(19, LOW);
  digitalWrite(pin, LOW);
  delayMicroseconds(Toff);
}

void PulsePinWithShiftSafe(MCP23017& mcpShift, uint8_t shiftPin, MCP23017& mcpKey, uint8_t keyPin) {
  mcpShift.digitalWrite(shiftPin, HIGH);
  mcpKey.digitalWrite(keyPin, HIGH);
  digitalWrite(17, HIGH);
  delayMicroseconds(rising_warmup);
  digitalWrite(19, HIGH);
  delayMicroseconds(rising_interval);
  digitalWrite(17, LOW);
  delayMicroseconds(rising_high);
  digitalWrite(19, LOW);
  mcpKey.digitalWrite(keyPin, LOW);
  mcpShift.digitalWrite(shiftPin, LOW);
  delayMicroseconds(Toff);
}

// ========== 攻击函数 ==========
void AttackKey(const char* keyName) {
  const char* actualKey = keyName;
  bool shiftNeeded = false;

  for (int i = 0; i < shiftedKeyCount; i++) {
    if (strcmp(shiftedKeys[i].shiftedChar, keyName) == 0) {
      actualKey = shiftedKeys[i].baseKey;
      shiftNeeded = true;
      break;
    }
  }

  uint8_t shiftPin = 7;  // keyMap3[7]
  MCP23017& shiftMCP = mcp3;

  for (int i = 0; i < 16; i++) {
    if (strcmp(keyMap1[i], actualKey) == 0) {
      shiftNeeded ? PulsePinWithShiftSafe(shiftMCP, shiftPin, mcp1, i) : PulsePin(mcp1, i);
      return;
    }
    if (strcmp(keyMap2[i], actualKey) == 0) {
      shiftNeeded ? PulsePinWithShiftSafe(shiftMCP, shiftPin, mcp2, i) : PulsePin(mcp2, i);
      return;
    }
    if (strcmp(keyMap3[i], actualKey) == 0) {
      shiftNeeded ? PulsePinWithShiftSafe(shiftMCP, shiftPin, mcp3, i) : PulsePin(mcp3, i);
      return;
    }
  }

  for (int i = 0; i < 3; i++) {
    if (strcmp(gpioKeys[i], actualKey) == 0) {
      PulseGPIO(gpioPins[i]);
      return;
    }
  }
}

// ========== 初始化 ==========
void setup() {
  Wire.begin();
  Serial.begin(115200);

  if (keyboardType == "Wooting 60 HE") cycle = 375;
  else if (keyboardType == "Steelseries Pro") cycle = 990;
  else if (keyboardType == "Keydous NJ98-CP") cycle = 5160;
  else if (keyboardType == "k70 pro max") cycle = 900;
  else if (keyboardType == "Reddragon M61") cycle = 1000;
  else if (keyboardType == "DrunkDeer A75pro") cycle = 880;

  rising_high = 2 * cycle;

  mcp1.init(); mcp2.init(); mcp3.init();
  mcp1.portMode(MCP23017Port::A, 0); mcp1.portMode(MCP23017Port::B, 0);
  mcp2.portMode(MCP23017Port::A, 0); mcp2.portMode(MCP23017Port::B, 0);
  mcp3.portMode(MCP23017Port::A, 0); mcp3.portMode(MCP23017Port::B, 0);
  mcp1.writePort(MCP23017Port::A, 0x00); mcp1.writePort(MCP23017Port::B, 0x00);
  mcp2.writePort(MCP23017Port::A, 0x00); mcp2.writePort(MCP23017Port::B, 0x00);
  mcp3.writePort(MCP23017Port::A, 0x00); mcp3.writePort(MCP23017Port::B, 0x00);

  for (int i = 0; i < 3; i++) {
    pinMode(gpioPins[i], OUTPUT);
    digitalWrite(gpioPins[i], LOW);
  }

  pinMode(17, OUTPUT); digitalWrite(17, LOW);
  pinMode(19, OUTPUT); digitalWrite(19, LOW);

  delay(3000);
}

// ========== 主循环：逐个攻击 A~Z ==========
void loop() {
  const char* upperKeys[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z"
  };

  for (int i = 0; i < 26; i++) {
    Serial.print("Attacking capital letter: "); Serial.println(upperKeys[i]);
    AttackKey(upperKeys[i]);
  }
}