/**********************************************************************
  ESP32 Door Opener/Closer Controller
**********************************************************************/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

char bleCommand = '\0';                  // emergency stop: 'S'
bool deviceConnected = false;
bool restartAdvertising = false;
const int openedPin = 32;                // limit switch (normally open)
const int closedPin = 35;                // limit switch (normally open)
const int pinX = 33;                     // left H-bridge control (DO)
const int pinY = 25;                     // right H-bridge control (D1)
const int servoPin = 13;
const int echoPin1 = 14;
const int trigPin1 = 12;
const int echoPin2 = 26;
const int trigPin2 = 27;
const int engagedAngle = 180;
const int disengagedAngle = 0;
const float minDistance = 5.0;           // ultrasonic detection range (cm)
const unsigned long maxOpenTime = 5000;  // timeout safety
const unsigned long maxCloseTime = 5000; // timeout safety

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
void forward() { digitalWrite(pinX,HIGH); digitalWrite(pinY,LOW); } 
void reverse() { digitalWrite(pinX,LOW); digitalWrite(pinY,HIGH); }
void motorstop() { digitalWrite(pinX,LOW); digitalWrite(pinY,LOW); }
void motorbrake() { digitalWrite(pinX,HIGH); digitalWrite(pinY,HIGH); }

/**
 * @brief verify if emergency stop was detected
 */
bool emergencyStopRequested() { return bleCommand == 'S'; }

//===================================================================
// SERVO MOTOR CONTROL

/**
 * @brief set servo duty cycles to _ (i.e. _ degrees)
 */
void engage() {
  // TODO
}

/**
 * @brief set servo duty cycles to 0 (i.e. 0 degrees)
 */
void disengage() {
  // TODO
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
  float duration = pulseIn(echoPin,HIGH, 30000); // time (us); wait 30 ms for a response
  if (duration == 0) {
    Serial.println("Ultrasonic sensor failure");
    warning('U');
    return false;   // change to true for fail-safe behavior: stop if the sensor isn't read
  }
  float distance = duration * 0.034 / 2;// converts echo time to distance (cm)
  return distance <= minDistance;
}

/**
 * @brief check all obstruction sensors
 * 1. determine if we are opening or closing to check the appropriate sensor
 * 2. check ultrasonic1 if mode is 'O' and ultrasonic2 if mode is 'C'
 */
bool obstructionDetected(char mode) {
  if (mode == 'O') {
    return checkUltrasonic(trigPin1,echoPin1);
  } 
  else if (mode == 'C') {
    return checkUltrasonic(trigPin2,echoPin2);
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
    Serial.println("WARNING: Obstruction detected");
    sendPhoneMessage("WARNING: Obstruction detected, motor stopped");
  } 
  else if (w == 'T') {
    Serial.println("WARNING: Timeout fault, check limit switches");
    sendPhoneMessage("WARNING: Switch was not detected, check limit switch position");
  }
  else if (w == 'U') {
    Serial.println("WARNING: Ultrasonic sensor failure");
    sendPhoneMessage("WARNING: Ultrasonic sensor failed to read");
  }
  else if (w == 'L') {
    Serial.println("WARNING: Both limit switches active");
    sendPhoneMessage("WARNING: Both switches are pressed at the same time");
  }
  else {
    Serial.println("WARNING: Unknown fault");
    sendPhoneMessage("WARNING: Unknown fault");
  }
}

/**
 * @brief Opening door control loop
 * 1. check if door is open
 * 2. check obstruction before starting
 * 3. engage servo motor
 * 4. turn on DC motor
 * 5. check emergency stop, obstructions, if the door has been opened,
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
  if (obstructionDetected('O')) {
    Serial.println("Obstruction detected before opening");
    warning('O');
    return;
  }

  Serial.println("Engaging motor with servo...");
  engage();
  delay(200);   // allow mechanism to engage
  Serial.println("Opening door...");
  forward();

  unsigned long startTime = millis();

  while (true) {
    if (emergencyStopRequested()) {
      Serial.println("Emergency stop during opening");
      motorstop();
      disengage();
      return;
    }
    if (obstructionDetected('O')) {
      Serial.println("Obstruction detected during opening");
      motorbrake();
      delay(50);
      motorstop();
      warning('O');
      disengage();
      return;
    }
    if (invalidLimitState()) {
      Serial.println("Invalid limit switch state");
      warning('L');
      motorstop();
      disengage();
      return;
    }
    if (doorIsOpen()) {
      Serial.println("Door reached open position");
      motorstop();
      disengage();
      return;
    }
    if (millis() - startTime > maxOpenTime) {
      Serial.println("Open timeout reached, check limit switch position");
      motorstop();
      warning('T');
      disengage();
      return;
    }
    delay(10);
  }
}

/**
 * @brief Closing door control loop
 * 1. check if door is closed
 * 2. check obstruction before starting
 * 3. engage servo motor
 * 4. turn on DC motor
 * 5. check emergency stop, obstructions, if the door has been closed,
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
  if (obstructionDetected('C')) {
    Serial.println("Obstruction detected before closing");
    warning('O');
    return;
  }

  Serial.println("Engaging motor with servo...");
  engage();
  delay(200);   // allow mechanism to engage
  Serial.println("Closing door...");
  reverse();

  unsigned long startTime = millis();

  while (true) {
    if (emergencyStopRequested()) {
      Serial.println("Emergency stop during closing");
      motorstop();
      disengage();
      return;
    }
    if (obstructionDetected('C')) {
      Serial.println("Obstruction detected during closing");
      motorbrake();
      delay(50);
      motorstop();
      warning('O');
      disengage();
      return;
    }
    if (invalidLimitState()) {
      Serial.println("Invalid limit switch state");
      warning('L');
      motorstop();
      disengage();
      return;
    }
    if (doorIsClosed()) {
      Serial.println("Door reached closed position");
      motorstop();
      disengage();
      return;
    }
    if (millis() - startTime > maxCloseTime) {
      Serial.println("Close timeout reached, check limit switch position");
      motorstop();
      warning('T');
      disengage();
      return;
    }
    delay(10);
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
  digitalWrite(servoPin,LOW);
}

void loop() {
  if (checkConnection()) {
    if (bleCommand == 'O') {
      Serial.println("Processed command: O");
      openDoor();
      bleCommand = '\0';
    } 
    else if (bleCommand == 'C') {
      Serial.println("Processed command: C");
      closeDoor();
      bleCommand = '\0';
    } 
    else if (bleCommand == 'S') {
      Serial.println("Processed command: S");
      motorstop();
      disengage();
      bleCommand = '\0';
    } 
    else if (bleCommand != '\0') {
      Serial.println("Unknown command processed");
      bleCommand = '\0';
    }
  }
}