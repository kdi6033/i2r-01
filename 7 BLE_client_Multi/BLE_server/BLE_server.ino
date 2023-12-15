/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define CONNECTION_TIMEOUT 30000 // 30초 (30,000 밀리초)

// 전역 변수 선언
BLECharacteristic *pCharacteristic;
bool connected = false;
unsigned long lastConnectionTime=0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      connected = true;
      Serial.println("Client connected");
      lastConnectionTime = millis();  // 연결 시간 기록
      // 클라이언트에게 메시지 전송 : 전송처리되지 않음
      //pCharacteristic->setValue("클라이언트 연결됨");
      //pCharacteristic->notify();
    };

    void onDisconnect(BLEServer* pServer) {
      connected = false;
      Serial.println("Client disconnected");

      // 여기에 광고 재시작 로직을 추가합니다.
      BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
      pAdvertising->start();
      Serial.println("Advertising started");
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
  Serial.println("Starting BLE work!");

  BLEDevice::init("i2r-sever");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                     CHARACTERISTIC_UUID,
                     BLECharacteristic::PROPERTY_READ |
                     BLECharacteristic::PROPERTY_WRITE |
                     BLECharacteristic::PROPERTY_NOTIFY
                   );


  pCharacteristic->setValue("Hello World says Neil");
  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  BLEDevice::startAdvertising();

  // 아래 코드를 추가합니다.
  std::string btAddress = BLEDevice::getAddress().toString();
  Serial.print("블루투스 장치 주소: ");
  Serial.println(btAddress.c_str());
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop() {
  // put your main code here, to run repeatedly:
  // 1초마다 "test" 메시지를 클라이언트에게 보내는 코드

  if(connected) {
    pCharacteristic->setValue("서버1");
    pCharacteristic->notify(); // 클라이언트에게 변경 사항 알림
    Serial.println("test 보냄");
  }

  // 연결 상태를 주기적으로 확인하고 필요한 조치를 취함
  if (connected) {
    unsigned long currentTime = millis();
    // 만약 일정 시간 동안 아무런 활동이 없으면 연결을 해제할 수 있음
    if (currentTime - lastConnectionTime > CONNECTION_TIMEOUT) {
      // 연결 해제 로직
      // 예: pServer->disconnect(); // 이 코드는 실제 사용하는 BLE 라이브러리에 따라 다를 수 있음
    }
  }
  delay(2000);
}