#ifndef KEYLISTENER_H
#define KEYLISTENER_H

#include <Adafruit_MLX90393.h>
#include <Arduino.h>
#include <Types.h>
#include <Config.h>

// 外部C函数声明 - 神经网络推理
extern "C" {
    void predict_keypress(float input[24], int* predicted_label, float* confidence);
}

class KeyListener {
public:
    // Constructor and destructor
    KeyListener();
    ~KeyListener();

    // Core methods
    bool begin();                              // Initialize sensors and system
    void update();                             // Main update loop - read sensors and process data
    void reset();                              // Reset system state
    
    // Configuration methods
    void setKeyDetectedCallback(KeyDetectedCallback callback);    // Set key detection callback
    void setSlopeThreshold(float threshold);                     // Set slope threshold
    void setAmplitudeThreshold(float threshold);                 // Set amplitude threshold
    void setSampleRate(int rate);              // Set sample rate
    void setThresholds(float slopeThresh, float ampThresh);  // Set detection thresholds
    
    // Calibration mode functionality
    void setCalibrationParams(float dx, float dy, float theta);  // Set calibration parameters
    void enableCalibration(bool enable);                        // Enable/disable calibration mode
    bool isCalibrationEnabled() const;                          // Check if calibration is enabled
    
    // Status query methods
    bool isCalibrated() const;                 // Check calibration status
    bool isKeyDetected() const;                // Check if key is detected
    
    // Data access methods
    void getCurrentSensorData(SensorData data[NUM_SENSORS]);  // Get current sensor data
    void getFilteredData(float data[NUM_SENSORS][3]);         // Get filtered data
    void getOffsetValues(float offsets[NUM_SENSORS][3]);      // Get calibration offset values
    
private:
    // Sensor objects
    Adafruit_MLX90393 sensors[NUM_SENSORS];
    
    // Data buffers
    float rawData[NUM_SENSORS][3];           // Raw sensor data
    float filteredData[NUM_SENSORS][3];     // Moving average filtered data
    float calibratedData[NUM_SENSORS][3];   // Offset-corrected data
    float envelopeData[NUM_SENSORS][3];     // Envelope data (final processed values)
    float offsetValues[NUM_SENSORS][3];     // Calibration offset values
    
    // Filter state
    float filterBuffer[NUM_SENSORS][3][FILTER_WINDOW_SIZE];
    int filterIndex[NUM_SENSORS][3];
    int filterCount[NUM_SENSORS][3];
    
    // Calibration state
    float calibBuffer[NUM_SENSORS][3][CALIBRATION_WINDOW];
    int calibIndex[NUM_SENSORS][3];
    int calibCount[NUM_SENSORS];
    bool calibratedFlag;
    
    // Envelope calculation state
    static const int ENV_POINTS_BUFFER = 12;  // Envelope window size
    float envelopeBuffer[NUM_SENSORS][3][12];  // Fixed size
    int envelopeIndex[NUM_SENSORS][3];
    
    // Peak detection state
    PeakState peakStates[NUM_SENSORS];
    float previousValues[NUM_SENSORS];
    float recentValues[NUM_SENSORS][50];  // For dynamic baseline calculation
    int recentIndex[NUM_SENSORS];
    
    // System state
    bool systemInitialized;
    unsigned long dataCounter;
    
    // Key detection debouncing
    unsigned long lastKeypressTime;          // Last key detection time
    static const unsigned long KEYPRESS_COOLDOWN = 300;  // Key cooldown time (milliseconds)
    
    // Threshold parameters
    float slopeThreshold;
    float amplitudeThreshold;
    
    // Callback function
    KeyDetectedCallback keyDetectedCallback;
    
    // Calibration transformation parameters
    bool calibrationEnabled;
    float calibDx, calibDy, calibTheta;
    
    // Private methods - Coordinate transformation
    bool getKeyCoordinates(const char* key, float& x, float& y) const;
    const char* findNearestKey(float target_x, float target_y, float& minDistance) const;
    void transformDetectedKeyCoordinate(float input_x, float input_y, float& output_x, float& output_y) const;
    
    // 私有方法 - 信号处理
    bool initializeSensors();                     // 初始化所有传感器
    void readAllSensors();                        // 读取所有传感器数据
    float applyMovingAverage(int sensorId, int axis);  // 应用移动平均滤波
    void updateCalibration();                     // 更新校准状态
    void calculateOffsets(int sensorId);          // 计算指定传感器的偏移
    float computeStandardDeviation(float* buffer, int size);  // 计算标准差
    bool isCalibrationStable(int sensorId);       // 检查校准稳定性
    void printBaseOffsets();                      // 打印校准偏移值
    
    // 私有方法 - 包络和峰值检测
    void calculateEnvelope();                     // 计算包络数据
    void detectPeaks();                           // 执行峰值检测
    void updateDynamicBaseline(int sensorId, float totalField);  // 更新动态基线
    void processPeakDetection(int sensorId, float totalField, float slope);  // 处理峰值检测逻辑
    
    // 私有方法 - 机器学习
    void predictKeypress(float peakData[8][3]);   // 执行按键预测
    bool canTriggerKeypress();                    // 检查是否可以触发按键检测
};

// 计算的常量
extern const int ENV_POINTS;
extern const int HALF_ENV;

#endif // KEYLISTENER_H 