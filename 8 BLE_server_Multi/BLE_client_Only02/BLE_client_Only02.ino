#include "BLEDevice.h"
//#include "BLEScan.h"


#define TRIGGER_PIN 34 // trigger pin GPIO36:i2r-04 GPIO34:i2r-03
const int ledPin = 2;

// Define the Data structure
struct DataDevice {
  int selectedCommMethod = -1; // 통신 방식 선택 변수, 초기값은 -1로 설정
  String mac=""; // Bluetooth mac address 를 기기 인식 id로 사용한다.
  String sendData=""; // 보드의 입력,출려,전압 데이터를 json 형태로 저장
};

struct DataBle {
  char *service_uuid="4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  char *characteristic_uuid="beb5483e-36e1-4688-b7f5-ea07361b26a8";
  bool isConnected=false;
};

// Create an instance of the Data structure
DataDevice dev;
DataBle ble;

unsigned int counter = 0;
unsigned long lastTime = 0;  // 마지막으로 코드가 실행된 시간을 기록할 변수
const long interval = 10000;  // 실행 간격을 밀리초 단위로 설정 (3초)
String returnMsg="";
unsigned long lastAttemptTime = 0;
const long reconnectInterval = 5000; // 5초 간격으로 재연결 시도

static std::string targetMacAddress = "b0:a7:32:1d:01:e6";
//static std::string targetMacAddress = "48:e7:29:37:c4:b6";
//static std::string targetMacAddress = "a0:b7:65:cd:4b:4a";
// The remote service we wish to connect to.
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

void setupClientBLE();

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("Received Data: ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)pData[i]);
    }
    Serial.println();
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    connected = true;
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    // MAC 주소를 기반으로 서버에 연결
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");
    pClient->setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)
  
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");


    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic.
    if(pRemoteCharacteristic->canRead()) {
      std::string value = pRemoteCharacteristic->readValue();
      Serial.print("The characteristic value was: ");
      Serial.println(value.c_str());
    }

    if(pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallback);

    connected = true;
    return true;
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("Found Device: ");
    Serial.println(advertisedDevice.toString().c_str());

    // MAC 주소 확인
    if (advertisedDevice.getAddress().toString() == targetMacAddress) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      Serial.println("=============");
      Serial.println("Device found:");
      Serial.print("Address: ");
      Serial.println(myDevice->getAddress().toString().c_str()); // MAC 주소 출력
      doConnect = true;
      doScan = true;
    }
  }
};

void setup() {
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);
  setupClientBLE();
} // End of setup.

void setupClientBLE() {
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("cli-02");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

  // 블루투스 장치 주소
  std::string btAddress = BLEDevice::getAddress().toString();
  Serial.print("블루투스 장치 주소: ");
  Serial.println(btAddress.c_str());
  Serial.println("Waiting for a client connection to notify...");
}

//1초 마다 실행되는 시간함수
void doTick() {
  unsigned long currentTime = millis();  // 현재 시간을 가져옵니다
  String strIn,strOut;
  if ( currentTime - lastTime >= interval) {
    lastTime = currentTime;
      // If we are connected to a peer BLE Server, update the characteristic each time we are reached
      // with the current time since boot.

      if (connected) {
        String newValue = "Time since boot: " + String(millis()/1000);
        Serial.println("Setting new characteristic value to \"" + newValue + "\"");
        
        // Set the characteristic's value to be the array of bytes that is actually a string.
        pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
      }else if(doScan){
        BLEDevice::getScan()->start(0);  // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
      }

  }
}

void startScan() {
  BLEDevice::getScan()->start(5, false);
}

void loop() {
  // 기존 doTick() 함수 호출 부분을 주석 처리합니다.
  // doTick();

  // 연결이 끊어졌고, 재연결 시도 시간이 되었을 때
  if (!connected && millis() - lastAttemptTime > reconnectInterval) {
    lastAttemptTime = millis(); // 마지막 시도 시간 업데이트
    doConnect = true; // 연결 시도 플래그 설정
    startScan(); // 스캔 시작
  }

  // 서버에 연결을 시도
  if (doConnect) {
    if (connectToServer()) {
      Serial.println("Connected to the BLE Server.");
      connected = true; // 연결 상태 업데이트
      doConnect = false; // 연결 시도 플래그 초기화
    } else {
      Serial.println("Failed to connect. Retry...");
      doConnect = false; // 연결 시도 플래그 초기화
      startScan(); // 재스캔 시작
    }
  }
}



