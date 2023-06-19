#include <ArduinoJson.h>
#include "CRC.h"
#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "i2r";
const char* password = "00000000";
//const char* mqtt_server = "192.168.0.2";
//IPAddress mqtt_server(192, 168, 0, 2);
const char* mqtt_server = "broker.mqtt-dashboard.com";
const char* outTopic = "/i2r/outTopic"; 
const char* inTopic = "/i2r/inTopic"; 
const char* clientName = "";

unsigned long currentMillis=0;
unsigned long previousMillis = 0;     
const long interval = 1000;  
String sMac;
int value;
char msg[200];
int mqttConnected=0; // 1=연결 0=끊김

int outPlc=1;
int Out[8]={0},In[10]={0};  // plc 입력과 출력 저장 
// pls return 메세지 처리
String sIn="";  // 받은 문자열
int serial2InTime=0;
String strIn="00000000",strInPre="00000000"; // In[] 을 string으로 저장

WiFiClient espClient;
PubSubClient client(espClient);

//json을 위한 설정
StaticJsonDocument<200> doc;
DeserializationError error;
JsonObject root;

void bootWifiStation();
void callback(char* topic, byte* payload, unsigned int length);
void crd16Rtu();
void doTick();
void reconnect();
void setup();
void serial2Event();
void publishMqtt();

const int ledPin = 2;

void setup() {
  pinMode(ledPin, OUTPUT);
  for(int i=0;i<5;i++) {
    digitalWrite(ledPin, 1);
    delay(500);
    digitalWrite(ledPin, 0);
    delay(500);
  }
  Serial.begin(19200);
  Serial2.begin(19200, SERIAL_8N1, 16, 17); //rs485

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

  String s;
  deserializeJson(doc,payload);
  root = doc.as<JsonObject>();
  const char* macIn = root["mac"];
  //if( sMac.equals(String(macIn))) {
    int no = root["no"];
    int value = root["value"];
    outPlc=1;
    Out[no]=value;
  //}

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
  serial2Event();
}

//1초 마다 실행되는 시간함수
void doTick() {
  if (!client.connected())
    reconnect();
  if(mqttConnected==1)
    client.loop();
 currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    crd16Rtu();
    //publishMqtt();
  }  
}

void publishMqtt()
{ 
  if(mqttConnected != 1)
    return;

  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();
  String json;
  root["mac"] = sMac;
  root["in0"] = In[0];
  root["in1"] = In[1];
  root["in2"] = In[2];
  root["in3"] = In[3];
  root["in4"] = In[4];
  root["in5"] = In[5];
  root["in6"] = In[6];
  root["in7"] = In[7];
  serializeJson(root, msg);
  client.publish(outTopic, msg);
  //Serial.println(msg);  
}

void serial2Event() {
  while(Serial2.available()) {
    char ch = Serial2.read();
    sIn+=ch;
    serial2InTime=currentMillis;
  }
  if((currentMillis - serial2InTime >= 500) && sIn.length() > 2) {
    for (int i = 0; i < sIn.length(); i++) {
      if(sIn[i]<16) Serial.print("0"); //padding zero for values less than 16
      Serial.print(sIn[i], HEX);
      Serial.print(" ");
    }
    Serial.println("");
    
    //입력을 저장하는 In[]에 저장
    int b=1;
    for(int i=1;i<=8;i++) {
      int c=sIn.charAt(3)&b;
      if(c!=0)
        c=0x01;
      In[i-1]=c;
      //Serial.print(c,HEX);
      //Serial.print(" ");
      b*=2;
    }

    sIn="";
    Serial.println("");

    strInPre=strIn;
    strIn=String(In[0])+String(In[1])+String(In[2])+String(In[3])+String(In[4])+String(In[5])+String(In[6])+String(In[7]);
    if( !strIn.equals(strInPre)) {
      publishMqtt();
      //Serial.println(strInPre);
      //Serial.println(strIn);
    }
  }
}

// 아두이노에서 RS485 출력을 내보낸다.
void crd16Rtu() {
  String s;
  int si,sj,len;
  char str[24];

  sIn=""; // plc 응답을 받는 string을 null로 만든다.
  if(outPlc == 1) {  //출력
    //str[24] =  {0x00,0x0f,0x00,0x00,0x00,0x0a,0x02,0xff,0x00,0x00,0x00};  //비트연속출력 len=9
    str[0]=0x00; str[1]=0x0f; str[2]=0x00; str[3]=0x00; str[4]=0x00;
    str[5]=0x0f; str[6]=0x02; str[7]=0xff; str[8]=0x00; str[9]=0x00; str[10]=0x00;
    len=9;
    str[7]=Out[0]+Out[1]*2+Out[2]*4+Out[3]*8+Out[4]*16+Out[5]*32;
    outPlc=0;
  }
  else {    //입력
    //str[24] =  {0x00,0x02,0x00,0x00,0x00,0x08,0x00,0x00}; // 비트 입력영역 읽기 len=6
    str[0]=0x00; str[1]=0x02; str[2]=0x00; str[3]=0x00; str[4]=0x00;
    str[5]=0x08; str[6]=0x00; str[7]=0x00; 
    len=6;
  }

  uint8_t * data = (uint8_t *) &str[0];
  si=crc16(data, len, 0x8005, 0xFFFF, 0x0000, true,  true  );
  sj=si&0xff;
  str[len]=sj;
  sj=si>>8;
  str[len+1]=sj;

  for(int i=0;i<len+2;i++)
    Serial2.print(str[i]);
}

