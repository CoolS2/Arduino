/**
   Датчик SR-04P для определения остатка гранул в бункере, датчики температуры для котла и бойлера

   static const uint8_t D0   = 16;
   static const uint8_t D1   = 5;
   static const uint8_t D2   = 4;
   static const uint8_t D3   = 0;
   static const uint8_t D4   = 2;
   static const uint8_t D5   = 14;
   static const uint8_t D6   = 12;
   static const uint8_t D7   = 13;
   static const uint8_t D8   = 15;
   static const uint8_t D9   = 3;
   static const uint8_t D10  = 1;
*/

#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
//#include <ArduinoJson.h>

//SR-04
const int trigPin = 13;  //D7
const int echoPin = 12;  //D6

//DALLAS
#define PIN_DS18B20 5 //D1
int countDallasSensors;

//VARIABLES
long SR_Duration;
int SR_Distance;
float tempBoiler;
float tempPot;

//NETWORK
byte mqtt_server[] = { 192, 168, 1, 231 };
char buffer[10];
// WiFi credentials.
const char* WIFI_SSID = "#";
const char* WIFI_PASS = "#";

const char* MQTT_SSID = "#";
const char* MQTT_PASS = "#";


OneWire oneWire(PIN_DS18B20);
DallasTemperature dallasSensors(&oneWire);

void MQTTMessage(char* topic, byte* payload, unsigned int length) {

}

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, MQTTMessage, wifiClient);

void connectWIFI() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long wifiConnectStart = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);

    // Only try for 5 seconds.
    if (millis() - wifiConnectStart > 5000) {
      return;
    }
  }
}

void reconnectMQTT() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("Reconnect to MQTT");
    client.connect("ESP8266 Thermostate", MQTT_SSID, MQTT_PASS);
    client.publish("outTopic", "connecting..");
    delay(5000);
  }
}

void getDistance() {
  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Reads the echoPin, returns the sound wave travel time in microseconds
  SR_Duration = pulseIn(echoPin, HIGH);

  // Calculating the distance
  SR_Distance = SR_Duration * 0.034 / 2;
}

void getTemperature(){
  dallasSensors.requestTemperatures(); 
  delay(1000);
  tempBoiler = dallasSensors.getTempCByIndex(0);
  tempPot = dallasSensors.getTempCByIndex(1);
}

void sendToServer(int distance, double tempBoiler, double tempPot, int countDallasSensors) {
  client.publish("outTopic", "connected");
  if(distance < 150 && distance > 0){
    client.publish("ESP/ToHome/distance", String(distance).c_str());
  }
  if(tempBoiler < 150 && tempBoiler > 0){
    client.publish("ESP/ToHome/tempBoiler", String(tempBoiler).c_str());
  }
  if(tempPot < 150 && tempPot > 0){
    client.publish("ESP/ToHome/tempPot", String(tempPot).c_str());
  }

  client.publish("ESP/ToHome/countDallasSensors", String(countDallasSensors).c_str());
  Serial.println("Reported!");
  delay(1000);
}

void toDeepSleep(){
    client.disconnect();
    WiFi.disconnect();
    delay(1000);
    Serial.println("Sleeping..");
    delay(20);
    ESP.deepSleep(60e6);
    
}

void setup() {
  Serial.begin(9600);

  // Wait for serial to initialize.
  while (!Serial) { }
  
  dallasSensors.begin();
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  countDallasSensors = dallasSensors.getDeviceCount();
  connectWIFI();
  
  Serial.println("Device Started");
  Serial.println("-------------------------------------");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWIFI();
  }

  if (!client.connected()) {
    reconnectMQTT();
  }

  if (client.connected()) {
    getDistance();
    getTemperature();
    sendToServer(SR_Distance, tempBoiler, tempPot, countDallasSensors);
    toDeepSleep();
  }
  client.loop();
}
