#include "CRC.h"

unsigned long currentMillis=0;
unsigned long previousMillis = 0;     
const long interval = 5000;  
String sMac;
int value;
char msg[200];

String sIn="";
int outPlc=1;
int Out[8]={0},In[10]={0};  // plc 입력과 출력 저장 
// pls return 메세지 처리
int serial2InTime=0;
String strIn="00000000",strInPre="00000000"; // In[] 을 string으로 저장

void crd16Rtu();
void doTick();
void setup();
void serial2Event();


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
}

void loop() {
  doTick();
  serial2Event();
}

//1초 마다 실행되는 시간함수
void doTick() {
  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    crd16Rtu();
    //tickMqtt();
  }  
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
      Serial.print(c,HEX);
      Serial.print(" ");
      b*=2;
    }

    sIn="";
    Serial.println("");

    /*
    strInPre=strIn;
    strIn=String(In[0])+String(In[1])+String(In[2])+String(In[3])+String(In[4])+String(In[5])+String(In[6])+String(In[7]);
    Serial.println(strInPre);
    Serial.println(strIn);
    */
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
    str[5]=0x0a; str[6]=0x02; str[7]=0xff; str[8]=0x00; str[9]=0x00; str[10]=0x00;
    len=9;
    str[7]=Out[0]+Out[1]*2+Out[2]*4+Out[3]*8;
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

