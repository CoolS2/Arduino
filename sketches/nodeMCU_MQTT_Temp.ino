/*
  ESP8266 (NodeMCU) Boiler + Distance Monitor
  FIXED / MODERNIZED VERSION:
  ✔ MQTT via hostname (no hardcoded IP)
  ✔ Better WiFi reconnect
  ✔ MQTT connect result logging
  ✔ No silent failures
  ✔ Safe publish helper
  ✔ DeepSleep stability
  ✔ MQTT socket timeout
  ✔ Optional Last Will
  ✔ Better Dallas handling
  ✔ pulseIn timeout
  ✔ Serial debug
  ✔ Works better after router / HA IP changes

  IMPORTANT:
  GPIO16 (D0) MUST be connected to RST for DeepSleep wakeup
*/

#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// =========================
// PINOUT (NodeMCU)
// =========================
static const uint8_t D0  = 16;
static const uint8_t D1  = 5;
static const uint8_t D6  = 12;
static const uint8_t D7  = 13;

// =========================
// SR-04
// =========================
const int trigPin = D7;
const int echoPin = D6;

// =========================
// DALLAS
// =========================
#define PIN_DS18B20 D1

// =========================
// NETWORK
// =========================
const char* WIFI_SSID = "#";
const char* WIFI_PASS = "#";

const char* MQTT_USER = "#";
const char* MQTT_PASS = "#";

// Use hostname instead of IP
// Examples:
// homeassistant.local
// raspberrypi.local
const char* MQTT_HOST = "homeassistant.local";
const int MQTT_PORT = 1883;

// =========================
// VARIABLES
// =========================
long SR_Duration = 0;
int SR_Distance = 0;

float tempBoiler = -127;
float tempPot = -127;

int countDallasSensors = 0;

unsigned long lastReconnectAttempt = 0;

// =========================
// OBJECTS
// =========================
OneWire oneWire(PIN_DS18B20);
DallasTemperature dallasSensors(&oneWire);

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// =========================
// MQTT CALLBACK
// =========================
void MQTTMessage(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT Message [");
  Serial.print(topic);
  Serial.print("]: ");

  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  Serial.println();
}

// =========================
// SAFE PUBLISH
// =========================
bool publishValue(const char* topic, String value, bool retained = true) {
  bool result = client.publish(topic, value.c_str(), retained);

  Serial.print("Publish -> ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.print(value);
  Serial.print(" | ");
  Serial.println(result ? "OK" : "FAIL");

  return result;
}

// =========================
// WIFI
// =========================
bool connectWIFI() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println("Connecting to WiFi...");

  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long wifiConnectStart = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    // 15 sec timeout
    if (millis() - wifiConnectStart > 15000) {
      Serial.println("\nWiFi FAILED");
      return false;
    }
  }

  Serial.println("\nWiFi Connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  return true;
}

// =========================
// MQTT
// =========================
bool reconnectMQTT() {
  if (client.connected()) {
    return true;
  }

  Serial.println("Connecting to MQTT...");

  String clientId = "ESP8266-Thermostat-";
  clientId += String(ESP.getChipId());

  bool connected = client.connect(
    clientId.c_str(),
    MQTT_USER,
    MQTT_PASS,
    "ESP/ToHome/status",
    1,
    true,
    "offline"
  );

  if (connected) {
    Serial.println("MQTT Connected");

    publishValue("ESP/ToHome/status", "online");
    publishValue("ESP/ToHome/boot", "booted");

    return true;
  } else {
    Serial.print("MQTT FAILED, rc=");
    Serial.println(client.state());

    return false;
  }
}

// =========================
// DISTANCE
// =========================
void getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // timeout prevents blocking forever
  SR_Duration = pulseIn(echoPin, HIGH, 30000);

  if (SR_Duration == 0) {
    SR_Distance = -1;
    Serial.println("Distance timeout");
    return;
  }

  SR_Distance = SR_Duration * 0.034 / 2;

  Serial.print("Distance: ");
  Serial.println(SR_Distance);
}

// =========================
// TEMPERATURE
// =========================
void getTemperature() {
  dallasSensors.requestTemperatures();

  tempBoiler = dallasSensors.getTempCByIndex(0);

  if (countDallasSensors > 1) {
    tempPot = dallasSensors.getTempCByIndex(1);
  } else {
    tempPot = -127;
  }

  Serial.print("Boiler Temp: ");
  Serial.println(tempBoiler);

  Serial.print("Pot Temp: ");
  Serial.println(tempPot);
}

// =========================
// SEND
// =========================
void sendToServer() {
  if (SR_Distance > 0 && SR_Distance < 150) {
    publishValue("ESP/ToHome/distance", String(SR_Distance));
  }

  if (tempBoiler > -50 && tempBoiler < 150) {
    publishValue("ESP/ToHome/tempBoiler", String(tempBoiler, 2));
  }

  if (tempPot > -50 && tempPot < 150) {
    publishValue("ESP/ToHome/tempPot", String(tempPot, 2));
  }

  publishValue("ESP/ToHome/countDallasSensors", String(countDallasSensors));

  Serial.println("Reported!");
}

// =========================
// SLEEP
// =========================
void toDeepSleep() {
  Serial.println("Preparing Deep Sleep...");

  client.loop();
  delay(300);

  client.disconnect();
  WiFi.disconnect(true);

  delay(500);

  Serial.println("Sleeping for 60 seconds...");
  ESP.deepSleep(60e6);
}

// =========================
// SETUP
// =========================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("Device Booting...");
  Serial.println("-------------------------------------");

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  dallasSensors.begin();
  countDallasSensors = dallasSensors.getDeviceCount();

  Serial.print("Dallas sensors found: ");
  Serial.println(countDallasSensors);

  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(MQTTMessage);
  client.setSocketTimeout(10);

  connectWIFI();
}

// =========================
// LOOP
// =========================
void loop() {
  if (!connectWIFI()) {
    Serial.println("WiFi unavailable -> sleep");
    toDeepSleep();
  }

  if (!reconnectMQTT()) {
    Serial.println("MQTT unavailable -> sleep");
    toDeepSleep();
  }

  client.loop();

  getDistance();
  getTemperature();
  sendToServer();

  toDeepSleep();
}
