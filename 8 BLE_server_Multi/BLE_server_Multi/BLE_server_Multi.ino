#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <map>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
std::map<uint16_t, bool> deviceConnectedMap; // 클라이언트 연결 상태 관리
uint32_t value = 0;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
      uint16_t connId = param->connect.conn_id;
      deviceConnectedMap[connId] = true;
      BLEDevice::startAdvertising();

      // 클라이언트 연결 정보 출력
      Serial.print("Client Connected: ID = ");
      Serial.print(connId);

      char addr_str[18];
      esp_bd_addr_t remote_bda;
      memcpy(remote_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
      sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
              remote_bda[0], remote_bda[1], remote_bda[2],
              remote_bda[3], remote_bda[4], remote_bda[5]);
      Serial.println(addr_str);
    };

    void onDisconnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
      uint16_t connId = param->disconnect.conn_id;
      deviceConnectedMap.erase(connId);

      // 클라이언트 연결 해제 정보 출력
      Serial.print("Client Disconnected: ID = ");
      Serial.println(connId);
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      Serial.println("**** Received Value: ****");
      for (int i = 0; i < rxValue.length(); i++)
        Serial.print(rxValue[i]);
      Serial.println("");
    }
  }
};

void setup() {
  Serial.begin(115200);
  BLEDevice::init("ESP32");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  // 블루투스 장치 주소
  std::string btAddress = BLEDevice::getAddress().toString();
  Serial.print("블루투스 장치 주소: ");
  Serial.println(btAddress.c_str());
  Serial.println("Waiting for a client connection to notify...");
}

void loop() {
  bool shouldIncrement = false;

  for (auto &pair : deviceConnectedMap) {
    if (pair.second) {
      //pCharacteristic->setValue((uint8_t*)"test", 4);
      Serial.println(value);
      String s = String(value);
      pCharacteristic->setValue((uint8_t*)s.c_str(), s.length());
      pCharacteristic->notify(pair.first);
      shouldIncrement = true;
    }
  }

  if (shouldIncrement) {
    value++;
  }
  
  delay(5000);
}
