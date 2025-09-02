#include "Adafruit_MLX90393.h"

#define num 8  // Number of MLX90393 sensors

Adafruit_MLX90393 sensor[num]; // Array to hold sensor instances
int CS[] = {5, 7, 9, 10, 11, 15, 27, 28}; // Chip select pins for each sensor

float x[num]; // X-axis magnetic field readings
float y[num]; // Y-axis magnetic field readings
float z[num]; // Z-axis magnetic field readings

unsigned long data_counter = 0; // Counter for data packets sent

void setup()
{
  Serial.begin(1000000); 
  /*
    Start serial communication at 1,000,000 baud rate.
    A high baud rate is required to transmit large data packets (8 sensors × 3 axes × 4 bytes per float)
    at a high sampling rate (250 Hz). Lower baud rates will cause transmission bottlenecks.
  */

  while (!Serial)
  {
    delayMicroseconds(10); // Wait for serial to become available (especially important for certain boards)
  }
  delayMicroseconds(1000); // Brief delay to ensure serial setup is stable

  for (int i = 0; i < num; ++i)
  {
    sensor[i] = Adafruit_MLX90393(); // Create sensor instance

    // Initialize sensor with SPI using corresponding CS pin
    while (!sensor[i].begin_SPI(CS[i]))
    {
      Serial.print("No sensor ");
      Serial.print(i + 1);
      Serial.println(" found ... check your wiring?");
      delayMicroseconds(500); // Delay before retrying
    }
    Serial.print("Sensor ");
    Serial.print(i + 1);
    Serial.println(" found!");

    // Set oversampling rate to improve measurement accuracy
    while (!sensor[i].setOversampling(MLX90393_OSR_0))
    {
      Serial.print("Sensor ");
      Serial.print(i + 1);
      Serial.println(" failed to set oversampling!");
      delayMicroseconds(500);
    }
    delayMicroseconds(500); // Delay between configuration steps

    // Set digital filtering level to reduce noise
    while (!sensor[i].setFilter(MLX90393_FILTER_2))
    {
      Serial.print("Sensor ");
      Serial.print(i + 1);
      Serial.println(" failed to set filter!");
      delayMicroseconds(500);
    }
  }
}

void loop()
{
  uint8_t packet[102]; 
  /*
    Data packet format:
    [2 bytes header] + [4 bytes data counter] + [8 sensors × 3 axes × 4 bytes float] = 102 bytes
  */
  int index = 0;

  // Add packet header (2 bytes) to indicate start of a data frame
  packet[index++] = 0xAA;
  packet[index++] = 0xBB;

  // Add 4-byte data counter for tracking transmitted frames
  memcpy(packet + index, &data_counter, sizeof(data_counter));
  index += sizeof(data_counter);

  // Trigger one-shot magnetic field measurement on all sensors
  for (int i = 0; i < num; ++i)
  {
    sensor[i].startSingleMeasurement();
  }

  // Wait for all sensors to complete measurement
  // Conversion time depends on resolution/filter settings; add small safety margin (200 µs)
  delayMicroseconds(mlx90393_tconv[2][0] * 1000 + 200); 

  // Read each sensor's X, Y, Z values and append them to the packet
  for (int i = 0; i < num; ++i)
  {
    if (!sensor[i].readMeasurement(&x[i], &y[i], &z[i]))
    {
      // If sensor read fails, default values to 0.0
      x[i] = y[i] = z[i] = 0.0;
    }

    // Append float values to packet in order: X, Y, Z
    memcpy(packet + index, &x[i], sizeof(float));
    index += sizeof(float);
    memcpy(packet + index, &y[i], sizeof(float));
    index += sizeof(float);
    memcpy(packet + index, &z[i], sizeof(float));
    index += sizeof(float);
  }

  // Send the entire packet via Serial in binary form
  Serial.write(packet, sizeof(packet));

  // Increment data counter for the next packet
  data_counter++;
}


