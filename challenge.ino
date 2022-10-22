#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"

#include <SPI.h>
#include <Wire.h>
#include <SparkFunMLX90614.h>  // Biblioeca do Sensor Infravermelho
#include <LiquidCrystal_I2C.h>


LiquidCrystal_I2C display(0x27, 16, 2);  //porta hexa, 16 colunas e 2 linhas


#define AWS_IOT_PUBLISH_TOPIC "esp32/2/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"
long lastReconnectAttempt = 0;
long connectWiFiCount = 0;
int reconnectToWifiDefault = 0;

IRTherm temperatura;
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);



void connectWifi(const char* ssid, const char* password) {

  connectWiFiCount = 0;

  Serial.println("Wi-Fi: " + String(ssid));
  display.clear();
  display.print("Conectando Wifi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("Connecting to Wi-Fi: ");
  display.setCursor(0, 1);
  display.print(String(ssid));

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(".");
    display.print(".");
    connectWiFiCount++;

    if (connectWiFiCount == 15) {
      connectWiFiCount = 0;
      display.clear();
      display.print("Falha Conexao!");
      display.setCursor(0, 1);
      display.print(String(ssid));
      delay(500);
      reconnectToWifiDefault++;

      if (reconnectToWifiDefault == 2) {
        reconnectToWifiDefault = 0;
        connectWifi("#", "12345678");
      } else {
        connectWifi(ssid, password);
      }
    }
  }

  Serial.println("Wi-Fi: " + String(ssid) + " connected!!");
  display.clear();
  display.print("Rede [" + String(ssid) + "]");

  display.setCursor(0, 1);
  display.print("Conectado!");
  delay(1000);


  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
}

boolean connectAWS() {


  Serial.println("RECONECTAR THING");
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);

  Serial.println("Connecting to AWS IOT");
  display.clear();
  display.print("Conectando no ");
  display.setCursor(0, 1);
  display.print("IoT Remoto...");

  Serial.println("AWS IoT Connected!");
  display.print("AWS IoT Conectado!");

  sendAndReceive();

  return client.connected();
}

boolean sendAndReceive() {
  if (client.connect(THINGNAME)) {
    //Serial.print("Thing founded...");
    publishMessage();
  } else {
    Serial.println("Falha na Conexão!");  //Exibe a mensagem de falha
    Serial.println(client.state());       // exibe o código pelo qual não foi possível conectar
    /*
    * -4 : MQTT_CONNECTION_TIMEOUT - the server didn't respond within the keepalive time
    * -3 : MQTT_CONNECTION_LOST - the network connection was broken
    * -2 : MQTT_CONNECT_FAILED - the network connection failed
    * -1 : MQTT_DISCONNECTED - the client is disconnected cleanly
    * 0 : MQTT_CONNECTED - the client is connected
    * 1 : MQTT_CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT
    * 2 : MQTT_CONNECT_BAD_CLIENT_ID - the server rejected the client identifier
    * 3 : MQTT_CONNECT_UNAVAILABLE - the server was unable to accept the connection
    * 4 : MQTT_CONNECT_BAD_CREDENTIALS - the username/password were rejected
    * 5 : MQTT_CONNECT_UNAUTHORIZED - the client was not authorized to connect
    */
  }

  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    Serial.println(client.state());
    Serial.println("");

    display.clear();
    display.print("Erro! Tempo esgotado!");
    display.setCursor(0, 1);
    display.print("cod erro: " + String(client.state()));
    delay(3000);
    return false;
  }

  // Subscribe to a topic
  if (!client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC)) {
    Serial.println("subscribe fails!");
  }

  client.setCallback(callback);
}

void setup() {
  Serial.begin(115200);
  display.init();
  display.backlight();

  display.print("Iniciando Sensor");

  if (temperatura.begin()) {
    display.clear();
    display.print("Sensor Ok!");
  } else {
    display.print("Sensor/Problema1");
    return;
  }
  delay(1000);

  // Inicia o Sensor no endereço 0x5A
  temperatura.setUnit(TEMP_C);  // Define a temperatura em Celsius

  if (!temperatura.read()) {
    display.print("Sensor/Problema2");
    display.clear();
    return;
  }

  delay(1000);
  display.print("Sensor Ok!");
  display.clear();

  lastReconnectAttempt = 0;

  delay(1500);
}


void loop() {

  if (!temperatura.read()) {
    Serial.println("mlx90614 fails...");
    display.print("Sensor/Problema");
    display.clear();
    return;
  }

  if (!WiFi.isConnected()) {
    Serial.println("wifi lose...");
    return connectWifi((const char*)"#", (const char*)"12345678");
  }

  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      Serial.println("now - 5000");
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (connectAWS()) {
        Serial.println("reconnecting...");
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    sendAndReceive();
    client.loop();
  }
}


void sensor() {
  if (temperatura.read()) {

    // temperatura.wake();
    display.clear();  // Posiciona o cursor

    display.print("Objeto: " + String(temperatura.object(), 2));
    Serial.println("Objeto: " + String(temperatura.object(), 2));

    display.setCursor(0, 1);  // Posiciona o cursor
    display.print("Ambiente: " + String(temperatura.ambient(), 2));
    Serial.println("Ambiente: " + String(temperatura.ambient(), 2));
    //temperatura.sleep();
    delay(1000);

    if (temperatura.object() > 34) {
      display.clear();
      display.print("Alerta!");
      display.setCursor(0, 1);
      display.print("Notificando....");
      delay(3000);
    }


  } else {
    Serial.println("temperature fails!! ");
  }
}

void publishMessage() {

  sensor();

  display.clear();
  display.print("Enviando dados...");


  StaticJsonDocument<200> doc;
  doc["ambient"] = String(temperatura.ambient(), 2).toFloat();
  doc["temperature"] = String(temperatura.object(), 2).toFloat();
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);  // print to client

  display.clear();
  display.print(String(jsonBuffer));

  if (client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer)) {
    Serial.println("publish ok!");
    display.clear();
    display.print("pub: " + String(AWS_IOT_PUBLISH_TOPIC));
    delay(2000);
  } else {
    Serial.println("publish error!");
  }
}


void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("=========================== Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: (length) " + String(length) + "  :  ");
  String payload;

  for (int i = 0; i < length; i++) {
    payload += (char)message[i];
  }

  Serial.println(String(payload));

  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload);

  display.clear();
  display.print("Nova Rede...");

  connectWifi(doc["ssid"], doc["password"]);
  Serial.println();
}
