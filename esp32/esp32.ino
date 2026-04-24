/**********************************************************************
  ESP32 Door Opener/Closer Controller
**********************************************************************/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

char bleCommand = '\0';                  
bool deviceConnected = false;
bool restartAdvertising = false;
bool disengaged = false;
bool setupModeDisengaged = false;
bool motorRunning = false;
unsigned long overCurrentStart = 0;                          // log when over current was detected
unsigned long idleOverCurrentStart = 0;
const bool ultrasonicEnabled = false;      
const int INA_SDA = 27;                                      // Serial Data pin for I2C
const int INA_SCL = 14;                                      // Serial Clock pin for I2C
const int openedPin = 32;                                    // limit switch (normally open)
const int closedPin = 26;                                    // limit switch (normally open)
const int pinX = 33;                                         // left H-bridge control (DO)
const int pinY = 25;                                         // right H-bridge control (D1)
const int servoPin = 13;
const int echoPin1 = 34;
const int trigPin1 = 12;
const int echoPin2 = 2;
const int trigPin2 = 15;
const int engagedAngle = 5;
const int disengagedAngle = 100;
const float minDistance = 5.0;                               // ultrasonic detection range (cm)
const float peakCurrent_obst = 120.0;                        // tuned expected load (mA)
const float peakCurrent_norm = 0.3;                         // tuned expected load (mA)
const float motorCurrentLimit_obst = peakCurrent_obst * 1.5; // obstruction threshold (mA)
const float motorCurrentLimit_norm = peakCurrent_norm * 1.5; // normal door operation threshold (mA)
const unsigned long maxOpenTime = 9000;                      // timeout safety
const unsigned long maxCloseTime = 9000;                     // timeout safety
const unsigned long currentTripTime = 100;                   // time above limit before fault (ms)
const unsigned long motorStartupIgnoreTime = 300;            // ignore startup spike

Servo engagingServo;
Adafruit_INA219 ina219;

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLECharacteristic* pTxCharacteristic = nullptr;

//===================================================================
// BLUETOOTH LOW ENERGY (BLE)

/**
 * @brief listens for when a phone connects or disconnects from BLE.
 */
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("BLE client connected");
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    restartAdvertising = true;
    Serial.println("BLE client disconnected");
    Serial.println("Restart advertising requested");
  }
};

/**
 * @brief when phone sends a command read it and store it.
 */
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();

    if (value.length() > 0) {
      bleCommand = value[0];   // take first letter only
      Serial.print("BLE command received: ");
      Serial.println(bleCommand);
    }
  }
};

/**
 * @brief setup BLE functionality
 * 1. initialize bluetooth server
 * 2. define comunication characteristics between esp32 and phone
 * 3. make the esp32 discoverable to other devices
 */
void setupBLE(const char* bleName) {
  BLEDevice::init(bleName);

  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE
  );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("BLE ready");
  Serial.println("Advertising started");
  Serial.println("Waiting for connection...");
}

/**
 * @brief verify if phone is connected, otherwise advertise esp32
 */
bool checkConnection() {
  if (restartAdvertising && !deviceConnected) {
    restartAdvertising = false;
    delay(200);
    BLEDevice::startAdvertising();
    Serial.println("Advertising restarted");
  }
  if (deviceConnected) {
    return true;
  }
  return false;
}

/**
 * @brief allows to send phone messages from the esp32
 */
void sendPhoneMessage(const char* msg) {
  if (deviceConnected && pTxCharacteristic != nullptr) {
    pTxCharacteristic->setValue(msg);
    pTxCharacteristic->notify();

    Serial.print("Message sent: ");
    Serial.println(msg);
  }
}

//===================================================================
// LIMIT SWITCHES

/**
 * @brief read limit switch outputs
 */
bool doorIsOpen() { return digitalRead(openedPin) == LOW; }
bool doorIsClosed() { return digitalRead(closedPin) == LOW; }

/**
 * @brief check if both limitswitches are pressed; should not happen
 */
bool invalidLimitState() { return doorIsOpen() && doorIsClosed(); }

//===================================================================
// DC MOTOR CONTROL

/**
 * @brief H-bridge control functions to set the DC motor's spin
 */
void forward() { digitalWrite(pinX,HIGH); digitalWrite(pinY,LOW); motorRunning = true; } 
void reverse() { digitalWrite(pinX,LOW); digitalWrite(pinY,HIGH); motorRunning = true; }
void motorstop() { digitalWrite(pinX,LOW); digitalWrite(pinY,LOW); motorRunning = false; }
void motorbrake() { digitalWrite(pinX,HIGH); digitalWrite(pinY,HIGH); motorRunning = false; }

/**
 * @brief verify if emergency stop was detected
 */
bool emergencyStopRequested() { return bleCommand == 'S'; } 

//===================================================================
// SERVO MOTOR CONTROL

/**
 * @brief set servo angle to 100 degrees
 */
void engage() {
  engagingServo.write(engagedAngle);
  Serial.print("Servo engaged at angle: ");
  Serial.println(engagedAngle);
  disengaged = false;
}

/**
 * @brief set servo angle to 5 degrees
 */
void disengage() {
  engagingServo.write(disengagedAngle);
  Serial.print("Servo disengaged at angle: ");
  Serial.println(disengagedAngle);
  disengaged = true;
}

//===================================================================
//  OBSTRUCTION DETECTION

/**
 * @brief check ultrasonic sensor
 */
bool checkUltrasonic(const int trigPin, const int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin,HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin,LOW);
  float duration = pulseIn(echoPin,HIGH, 50000); // time (us); wait 50 ms for a response
  if (duration == 0) {
    Serial.println("No echo detected");
    // change to true for fail-safe behavior: stop if the sensor isn't read
    return false; // assume no obstruction
  }
  float distance = duration * 0.034 / 2; // converts echo time to distance (cm)
  return distance <= minDistance;
}

/**
 * @brief read current from INA219
 */
float readMotorCurrent_mA() {
  float current_mA = ina219.getCurrent_mA();

  // sometimes sensor noise can give tiny negative values
  if (current_mA < 0) {
    current_mA = -current_mA;
  }

  return current_mA;
}

/**
 * @brief control the rate a which the currents are printed in serial monitor;
 * for debug and tuning purposes
 */
void printCurrent(const char* label, float current) {
  static unsigned long lastPrintTime = 0;
  const unsigned long printInterval = 1000;

  if (millis() - lastPrintTime >= printInterval) {
    lastPrintTime = millis();

    Serial.print(label);
    Serial.print(": ");
    Serial.println(current);
  }
}

/**
 * @brief check if overcurrent conditions have been met
 * 1. don't account for startup spikes
 * 2. read current
 * 3. check if current exceeds limit
 * 4. check if the current spike is sustained
 */
bool overCurrentDetected(unsigned long motionStartTime) {
  // ignore normal motor startup spike
  if (millis() - motionStartTime < motorStartupIgnoreTime) {
    overCurrentStart = 0;
    return false;
  }

  float current_mA = readMotorCurrent_mA();

  printCurrent("Motor current mA", current_mA);

  if (current_mA > motorCurrentLimit_obst) {
    if (overCurrentStart == 0) {
      overCurrentStart = millis();
    }

    if (millis() - overCurrentStart >= currentTripTime) {
      return true;
    }
  } 
  else {
    overCurrentStart = 0;
  }

  return false;
}

/**
 * @brief check if someone is operating the door normally (i.e. pushing the door)
 */
bool idleLoadDetected() {
  float current_mA = readMotorCurrent_mA();

  printCurrent("Idle current mA", current_mA);

  if (!motorRunning && !disengaged) {
    if (current_mA > motorCurrentLimit_norm) {
      if (idleOverCurrentStart == 0) {
        idleOverCurrentStart = millis();
      }
      if (millis() - idleOverCurrentStart > 100) {
        return true;
      }
    } 
    else {
      idleOverCurrentStart = 0;
    }
  } 
  else {
    idleOverCurrentStart = 0;
  }

  return false;
}

/**
 * @brief check all obstruction sensors
 * 1. check current sensor
 * 2. determine if we are opening or closing to check the appropriate sensor
 * 3. check ultrasonic1 if mode is 'O' and ultrasonic2 if mode is 'C', and they are enabled
 */
bool obstructionDetected(char mode, unsigned long motionStartTime) {
  if (overCurrentDetected(motionStartTime)) {
    return true;
  }
  if (ultrasonicEnabled) {
      if (mode == 'O') {
      return checkUltrasonic(trigPin1,echoPin1);
    } 
    else if (mode == 'C') {
      return checkUltrasonic(trigPin2,echoPin2);
    }
  }
  return false;
}

//===================================================================
// CONTROL LOOPS

/**
 * @brief send warning to phone
 * 1. 'O': Obstruction detected, command stopped
 * 2. 'T': Timeout fault, check limit switches
 * 3. 'U': Ultrasonic sensor failure
 * 4. 'L': Both limit switches were detected simultaneously 
 */
void warning(char w) {
  if (w == 'O') {
    // WARNING: Obstruction detected
    sendPhoneMessage("WARNING: Obstruction detected, motor stopped");
  } 
  else if (w == 'T') {
    // WARNING: Timeout fault, check limit switches
    sendPhoneMessage("WARNING: Switch was not detected, check limit switch position");
  }
  else if (w == 'U') {
    // WARNING: Ultrasonic sensor failure
    sendPhoneMessage("WARNING: Ultrasonic sensor failed to read");
  }
  else if (w == 'L') {
    // WARNING: Both limit switches active
    sendPhoneMessage("WARNING: Both limit switches are pressed at the same time");
  }
  else {
    sendPhoneMessage("WARNING: Unknown fault");
  }
}

/**
 * @brief Opening door control loop
 * 1. check if door is open
 * 2. engage servo motor
 * 3. turn on DC motor
 * 4. check emergency stop, obstructions, if the door has been opened,
 *    if the limit switches have invalid behavior, and if the door went
 *    past what was expectected (i.e. missed limit switch)
 */
void openDoor() {
  Serial.println("Starting openDoor()");

  // clear start command so only a new 'S' matters during motion
  bleCommand = '\0';

  if (doorIsOpen()) {
    Serial.println("Door already open");
    return;
  }

  Serial.println("Opening door...");
  forward();

  unsigned long startTime = millis();

  while (true) {
    if (emergencyStopRequested()) {
      Serial.println("Emergency stop during opening");
      motorstop();
      return;
    }
    if (obstructionDetected('O', startTime)) {
      Serial.println("Obstruction detected during opening");
      motorbrake();
      delay(50);
      motorstop();
      disengage();
      warning('O');
      return;
    }
    if (invalidLimitState()) {
      Serial.println("Invalid limit switch state");
      warning('L');
      motorstop();
      return;
    }
    if (doorIsOpen()) {
      Serial.println("Door reached open position");
      motorstop();
      return;
    }
    if (millis() - startTime > maxOpenTime) {
      Serial.println("Open timeout reached, check limit switch position");
      motorstop();
      warning('T');
      return;
    }
    delay(10);
  }
}

/**
 * @brief Closing door control loop
 * 1. check if door is closed
 * 2. engage servo motor
 * 3. turn on DC motor
 * 4. check emergency stop, obstructions, if the door has been closed,
 *    if the limit switches have invalid behavior, and if the door went
 *    past what was expectected (i.e. missed limit switch)
 */
void closeDoor() {
  Serial.println("Starting closeDoor()");

  // clear start command so only a new 'S' matters during motion
  bleCommand = '\0';

  if (doorIsClosed()) {
    Serial.println("Door already closed");
    return;
  }
  
  Serial.println("Closing door...");
  reverse();

  unsigned long startTime = millis();

  while (true) {
    if (emergencyStopRequested()) {
      Serial.println("Emergency stop during closing");
      motorstop();
      return;
    }
    if (obstructionDetected('C', startTime)) {
      Serial.println("Obstruction detected during closing");
      motorbrake();
      delay(50);
      motorstop();
      disengage();
      warning('O');
      return;
    }
    if (invalidLimitState()) {
      Serial.println("Invalid limit switch state");
      warning('L');
      motorstop();
      return;
    }
    if (doorIsClosed()) {
      Serial.println("Door reached closed position");
      motorstop();
      return;
    }
    if (millis() - startTime > maxCloseTime) {
      Serial.println("Close timeout reached, check limit switch position");
      motorstop();
      warning('T');
      return;
    }
    delay(10);
  }
}

void setupWheelEngagement() {
  motorstop();

  if (!setupModeDisengaged) {
    Serial.println("Wheel setup step 1: disengaging servo.");
    Serial.println("Press motor/wheel down so gear locks in, then press SETUP again.");

    engagingServo.write(180);
    Serial.print("Servo disengaged at angle: ");
    Serial.println(180);
    disengaged = true;
    setupModeDisengaged = true;

    sendPhoneMessage("SETUP: Press wheel down, then press SETUP again");
  } 
  else {
    Serial.println("Wheel setup step 2: engaging servo.");

    engage();
    setupModeDisengaged = false;

    sendPhoneMessage("SETUP: Wheel engaged");
  }
}

//===================================================================
// MAIN

void setup() {
  Serial.begin(115200);
  delay(1000);
  setupBLE("ESP32_Bluetooth");
 
  pinMode(openedPin,INPUT_PULLUP); 
  pinMode(closedPin,INPUT_PULLUP); 

  pinMode(echoPin1,INPUT);
  pinMode(echoPin2,INPUT);
  pinMode(trigPin1,OUTPUT);
  pinMode(trigPin2,OUTPUT);
  digitalWrite(trigPin1,LOW);
  digitalWrite(trigPin2,LOW);

  pinMode(pinX,OUTPUT);
  pinMode(pinY,OUTPUT);
  digitalWrite(pinX,LOW);
  digitalWrite(pinY,LOW);

  pinMode(servoPin,OUTPUT);
  engagingServo.setPeriodHertz(50);          // standard servo frequency
  engagingServo.attach(servoPin, 500, 2500); // pulse range in microseconds

  Wire.begin(INA_SDA,INA_SCL);
  if (!ina219.begin()) {
    Serial.println("Failed to find INA219 current sensor");
    sendPhoneMessage("WARNING: INA219 not detected");
  } 
  else {
    Serial.println("INA219 current sensor ready");
  }

  engage();
  disengaged = false;

  delay(1000);
}

void loop() {
  if (idleLoadDetected()) {
    Serial.println("External force detected → disengaging");
    disengage();
    idleOverCurrentStart = 0;
    sendPhoneMessage("WARNING: External force detected");
  }
  if (checkConnection()) {
    if (bleCommand == 'O') {
      Serial.println("Processed command: O");
      engage();
      openDoor();
      bleCommand = '\0';
    } 
    else if (bleCommand == 'C') {
      Serial.println("Processed command: C");
      engage();
      closeDoor();
      bleCommand = '\0';
    } 
    else if (bleCommand == 'S') {
      Serial.println("Processed command: S");
      motorstop();
      bleCommand = '\0';
    } 
    else if (bleCommand == 'W') {
      Serial.println("Processed command: W");
      setupWheelEngagement();
      bleCommand = '\0';
    }
    else if (bleCommand != '\0') {
      Serial.println("Unknown command processed");
      bleCommand = '\0';
    }
  }
}