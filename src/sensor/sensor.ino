#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

#define FILTER_LENGTH 50
#define LOOP_DELAY 10

//sensor variables
const int noSensors = 2;
int pirPins[noSensors] = {10, 11}; // pin numbers
int outputPins[noSensors] = {6, 7}; // pins for output, same state as mqtt message
uint8_t button = BOOT_PIN;
#define OCCUPANCY_SENSOR_ENDPOINT_NUMBER 10

// struc to hold sensor data
typedef struct {
  int buf[FILTER_LENGTH];
  float percentage;
  int bufIndex;
  unsigned long outputTimer;
  ZigbeeOccupancySensor* zigBeeSensor;
  bool output;
} SensorObj;

SensorObj SensorData[noSensors];
ZigbeeOccupancySensor zbOccupancySensor1 = ZigbeeOccupancySensor(OCCUPANCY_SENSOR_ENDPOINT_NUMBER); 
ZigbeeOccupancySensor zbOccupancySensor2 = ZigbeeOccupancySensor(OCCUPANCY_SENSOR_ENDPOINT_NUMBER + 1); 

// initilaize struct for sensors
void Sensor_Init(SensorObj *sensor, ZigbeeOccupancySensor *zigOcc) {
  for (int n = 0; n < FILTER_LENGTH; n++) {
    sensor->buf[n] = 0;
  }
  sensor->percentage = 0.0;
  sensor->bufIndex = 0;
  sensor->outputTimer = 0;
  sensor->zigBeeSensor = zigOcc;
  sensor->output = 0;
}

// update struct for sensors
bool Sensor_Update(SensorObj *sensor, int id, int input) {
  bool old_output = sensor->output;
  sensor->buf[sensor->bufIndex] = input;

  sensor->bufIndex++;

  if (sensor->bufIndex == FILTER_LENGTH) {
    sensor->bufIndex = 0;
  }

  int pirOutput = 0;
  int trig_count = 0;
  for (int n = 0; n < FILTER_LENGTH; n++) {
    trig_count += sensor->buf[n];
  }

  sensor->percentage = (float)trig_count/FILTER_LENGTH * 100;

  if (sensor->percentage > 75.0) {
    pirOutput = 1;
  }

  if (pirOutput == 1) {
    sensor->outputTimer = millis();
    sensor->output = true;
  }

  int outputDelay = 5000;
  if ((millis() - sensor->outputTimer) > outputDelay) {
    sensor->output = false;
  }

  if (old_output != sensor->output) {
    digitalWrite(outputPins[id], sensor->output); // write to pin
    Serial.print("new_output");
    Serial.println(sensor->output);
    sensor->zigBeeSensor->setOccupancy(sensor->output);
    sensor->zigBeeSensor->report();

  }
  
  return sensor->output;
}

void setup() {
  // initialize serial
  Serial.begin(115200);
  delay(10);
  Serial.println("Serial online");

  // initialize sonsor pins and sensor struct
  for (int i  = 0; i < noSensors; i++) {
    pinMode(pirPins[i], INPUT_PULLDOWN); // testing with Panasonic EKMC1601111 which needs pulldown resistor
    pinMode(outputPins[i], OUTPUT);
  }

  zbOccupancySensor1.setManufacturerAndModel("Espressif", "ZigbeeOccupancyPIRSensor1");
  zbOccupancySensor2.setManufacturerAndModel("Espressif", "ZigbeeOccupancyPIRSensor2");
  Zigbee.addEndpoint(&zbOccupancySensor1);
  Zigbee.addEndpoint(&zbOccupancySensor2);
  Sensor_Init(&SensorData[0], &zbOccupancySensor1);
  Sensor_Init(&SensorData[1], &zbOccupancySensor2);

  // Optional: set Zigbee device name and model
  pinMode(button, INPUT_PULLUP);

  Serial.println("Starting Zigbee...");
  // When all EPs are registered, start Zigbee in End Device mode
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  } else {
    Serial.println("Zigbee started successfully!");
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
}

void loop() {

  for (int i = 0; i < noSensors; i++) {
    int pirState = digitalRead(pirPins[i]);
    Sensor_Update(&SensorData[i], i, pirState);
  }

  // Checking button for factory reset
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
  }

  delay(LOOP_DELAY);
}