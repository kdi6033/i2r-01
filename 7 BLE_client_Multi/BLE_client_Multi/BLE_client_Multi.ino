#include "BLEDevice.h"

// 서버의 MAC 주소 배열
const char* serverMacs[] = {"48:e7:29:37:c4:b6", "a0:b7:65:cd:4b:4a"};
const int numServers = sizeof(serverMacs) / sizeof(serverMacs[0]);
int currentServerIndex = 0; // 현재 연결 시도 중인 서버 인덱스
int counter = 0;

BLEClient* pClient = nullptr;
BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
BLERemoteCharacteristic* pRemoteCharacteristic;

bool connectToServer(const char* serverMac);
void disconnectFromServer();

void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  Serial.print("Received Data: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)pData[i]);
  }
  Serial.println();
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) override {
    Serial.println("Connected to Server");
  }

  void onDisconnect(BLEClient* pclient) override {
    Serial.println("Disconnected from Server");
  }
};

void setup() {
  Serial.begin(115200);
  BLEDevice::init("");

  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  // 첫 번째 서버에 연결
  connectToServer(serverMacs[currentServerIndex]);
}

bool connectToServer(const char* serverMac) {
  BLEAddress serverAddress(serverMac);
  if (pClient->connect(serverAddress)) {
    Serial.print("Connected to: ");
    Serial.println(serverMac);

    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.println("Failed to find our service UUID");
      return false;
    }

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.println("Failed to find our characteristic UUID");
      return false;
    }

    if(pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallback);

    // 여기에 데이터 처리 로직 추가
    // 예: pRemoteCharacteristic->writeValue("Hello World", true);

    return true;
  } else {
    Serial.print("Failed to connect to: ");
    Serial.println(serverMac);
    return false;
  }
}

void disconnectFromServer() {
  if (pClient->isConnected()) {
    pClient->disconnect();
  }
}

void loop() {
   // 다음 서버로 스위칭
  currentServerIndex = (currentServerIndex + 1) % numServers;
  disconnectFromServer();
  connectToServer(serverMacs[currentServerIndex]);
      counter++;
    String s=String(counter);
    pRemoteCharacteristic->writeValue(s.c_str(), s.length());

  delay(5000); // 일정한 시간 간격으로 서버를 스위칭합니다.
}

