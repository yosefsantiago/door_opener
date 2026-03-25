/**********************************************************************
  ESP32 Door Opener/Closer Controller
**********************************************************************/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

char bleCommand = '\0'; // emergency stop: 'S'
bool deviceConnected = false;
bool restartAdvertising = false;
const int openedPin = 32; // limit switch (normally open)
const int closedPin = 35; // limit switch (normally open)
const int pinX = 33;      // left H-bridge control (DO)
const int pinY = 25;      // right H-bridge control (D1)
const int echoPin1 = 14;
const int trigPin1 = 12;
const int echoPin2 = 26;
const int trigPin2 = 27;
const int servoPin = 13;

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

//===================================================================
// LIMIT SWITCHES

/**
 * @brief read limit switch outputs
 */
bool doorIsOpen() { return digitalRead(openedPin) == HIGH; }
bool doorIsClosed() { return digitalRead(closedPin) == HIGH; }

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
 * @brief check all obstruction sensors
 */
bool obstructionDetected() {
  // TODO
  return false;
}

//===================================================================
// CONTROL LOOPS

void warning(char w) {
  // TODO
}

/**
 * @brief Opening door control loop
 * 1. check if door is open
 * 2. check obstruction before starting
 * 3. engage servo motor
 * 4. turn on DC motor
 * 5. check emergency stop, obstructions, if the door has been opened, and
 *    if the door went past what was expectected (i.e. missed limit switch)
 */
void openDoor() {
  Serial.println("Starting openDoor()");

  // clear start command so only a new 'S' matters during motion
  bleCommand = '\0';

  if (doorIsOpen()) {
    Serial.println("Door already open");
    return;
  }
  if (obstructionDetected()) {
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
  const unsigned long maxOpenTime = 5000; // timeout safety

  while (true) {
    if (emergencyStopRequested()) {
      Serial.println("Emergency stop during opening");
      motorstop();
      disengage();
      return;
    }
    if (obstructionDetected()) {
      Serial.println("Obstruction detected during opening");
      motorbrake();
      delay(50);
      motorstop();
      warning('O');
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
      Serial.println("Open timeout reached");
      motorstop();
      warning('T');
      disengage();
      return;
    }
    delay(10);
    
  }
}

void closeDoor() {
  // TODO
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
    } else if (bleCommand == 'C') {
      Serial.println("Processed command: C");
      closeDoor();
      bleCommand = '\0';
    } else if (bleCommand == 'S') {
      Serial.println("Processed command: S");
      motorstop();
      disengage();
      bleCommand = '\0';
    } else if (bleCommand != '\0') {
      Serial.println("Unknown command processed");
      bleCommand = '\0';
    }
  }
}