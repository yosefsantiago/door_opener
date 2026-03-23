/**********************************************************************
  ESP32 Door Opener/Closer Controller
**********************************************************************/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

char bleCommand = '\0';
bool deviceConnected = false;
bool restartAdvertising = false;
bool isOpen = false;
bool isClosed = false;
bool motorOn = false;
bool disengaged = false; // servo control
const int openedPin = 32;
const int closedPin = 35;
const int pinX = 33; // left H-bridge control
const int pinY = 25; // right H-bridge control
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
 * @brief Setup BLE functionality
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
// CONTROL LOOPS

void openDoor() {}

void closeDoor() {}

void resetDoor() {}

void warning() {}

void setup() {
  Serial.begin(115200);
  delay(1000);
  setupBLE("ESP32_Bluetooth");
 
  pinMode(openedPin,INPUT); 
  pinMode(closedPin,INPUT); 

  pinMode(echoPin1,INPUT);
  pinMode(echoPin2,INPUT);
  pinMode(trigPin1,OUTPUT);
  pinMode(trigPin2,OUTPUT);
  digitalWrite(trigPin1,LOW);
  digitalWrite(trigPin2,LOW);
}

void loop() {
  if (checkConnection()) {

    if (bleCommand != '\0') {
      Serial.print("Processed command: ");
      Serial.println(bleCommand);
    } else if (bleCommand == 'O') {
      openDoor();
    } else if (bleCommand == 'C') {
      closeDoor();
    } else if (bleCommand == 'R') {
      resetDoor();
    } else {
      warning();
    }
  }

  // analogWrite(27,125);
  // analogWrite(26,125);
  // analogWrite(25,125);
  // analogWrite(33,125);
}