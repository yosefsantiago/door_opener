/**********************************************************************
  ESP32 BLE receive text from phone/web app and print to Serial
**********************************************************************/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

bool deviceConnected = false;
String receivedText = "";

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLECharacteristic* pTxCharacteristic = nullptr;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("BLE client connected");
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("BLE client disconnected");
    pServer->getAdvertising()->start();
    Serial.println("Advertising restarted");
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();

    if (value.length() > 0) {
      receivedText = "";
      for (size_t i = 0; i < value.length(); i++) {
        receivedText += (char)value[i];
      }

      Serial.print("Received via BLE: ");
      Serial.println(receivedText);
    }
  }
};

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

void setup() {
  Serial.begin(115200);
  delay(1000);
  setupBLE("ESP32_Bluetooth");
}

void loop() {
  if (receivedText.length() > 0) {
    String cmd = receivedText;
    receivedText = "";
    cmd.trim();

    Serial.print("Processed command: ");
    Serial.println(cmd); 
  }
}