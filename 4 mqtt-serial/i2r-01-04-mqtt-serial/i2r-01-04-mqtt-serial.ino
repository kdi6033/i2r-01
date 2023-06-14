#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Update these with values suitable for your network.

const char* ssid = "i2r";
const char* password = "00000000";
//const char* mqtt_server = "192.168.0.2";
//IPAddress mqtt_server(192, 168, 0, 2);
const char* mqtt_server = "broker.mqtt-dashboard.com";
const char* outTopic = "/i2r/outTopic"; 
const char* inTopic = "/i2r/inTopic"; 
const char* clientName = "";

unsigned long previousMillis = 0;     
const long interval = 5000;  
String sMac;
int value;
char msg[200];
int mqttConnected=0; // 1=연결 0=끊김

WiFiClient espClient;
PubSubClient client(espClient);

//json을 위한 설정
StaticJsonDocument<200> doc;
DeserializationError error;
JsonObject root;

void bootWifiStation();
void callback(char* topic, byte* payload, unsigned int length);
void doTick();
void reconnect();
void setup();
void serial1Event();
void serial2Event();
void tickMqtt();

const int ledPin = 2;

void setup() {
  pinMode(ledPin, OUTPUT);
  for(int i=0;i<5;i++) {
    digitalWrite(ledPin, 1);
    delay(500);
    digitalWrite(ledPin, 0);
    delay(500);
  }
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 26, 27); //rs232
  Serial2.begin(115200, SERIAL_8N1, 16, 17); //rs485

    //이름 자동으로 생성
  uint8_t chipid[6]="";
  WiFi.macAddress(chipid);
  char* cChipID = new char[13];
  sprintf(cChipID,"%02x%02x%02x%02x%02x%02x%c",chipid[5], chipid[4], chipid[3], chipid[2], chipid[1], chipid[0],0);
  sMac=String(cChipID);
  clientName = sMac.c_str();
  Serial.println("");
  Serial.println(clientName);

  bootWifiStation();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void bootWifiStation() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  memcpy(msg, payload, length); // payload를 msg 배열에 복사
  msg[length] = '\0'; // 문자열의 끝을 표시
  Serial1.print(msg);
}

// mqtt 통신에 지속적으로 접속한다.
void reconnect() {
  if(WiFi.status() != WL_CONNECTED)
    return;
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientName)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish(outTopic, "Reconnected");
      // ... and resubscribe
      client.subscribe(inTopic);
      mqttConnected=1;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      mqttConnected=0;
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  doTick();
  serial1Event();
  serial2Event();
}

//1초 마다 실행되는 시간함수
void doTick() {
    if (!client.connected()) {
    reconnect();
  }
  if(mqttConnected==1)
  client.loop();
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    tickMqtt();
  }  
}

void tickMqtt()
{ 
  if (!client.connected()) {
    reconnect();
  }
  if(mqttConnected != 1)
    return;

  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();
  String json;
  root["value"] = value++;
  serializeJson(root, msg);
  client.publish(outTopic, msg);
  //Serial.println(msg);  
}

void serial1Event() {
  while(Serial1.available()) {
    char ch = Serial1.read();
    Serial.write(ch);
  }
}

void serial2Event() {
  while(Serial2.available()) {
    char ch = Serial2.read();
    Serial.write(ch);
  }
}

