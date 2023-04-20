#include <WiFi.h>
#include <PubSubClient.h>

// Wi-Fi 정보 입력
const char* ssid = "i2r";
const char* password = "00000000";

// MQTT 브로커 정보 입력
const char* mqtt_broker = "broker.mqtt-dashboard.com";
const int mqtt_port = 1883;
const char* topic_in = "kdi/in";
const char* topic_out = "kdi/out";
const char* client_name = "123rrrrr";

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");

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

  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(client_name)) {
      Serial.println("connected");
      client.subscribe(topic_in);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // 메시지 보내기 (예시)
  static unsigned long last_send = 0;
  if (millis() - last_send > 5000) {
    last_send = millis();
    client.publish(topic_out, "Hello from ESP32");
  }
}
