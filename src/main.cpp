#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <FirebaseESP8266.h>
#include <PubSubClient.h>

// dht 11
#include "DHT.h"
#define DHTPIN 5 // what digital pin we're connected to
#define DHTTYPE DHT11 // DHT 11
DHT dht(DHTPIN, DHTTYPE);

// relay
#define RELAYPIN 0

// power sensor
#define sensorPower A0

// switch button
#define SWITCH_PIN 16
boolean button = false;

#define FIREBASE_KEY "86xZRKvjOOhz9UAVYDJ4X3gh5gFbvZzV7ZobwPjl"
#define FIREBASE_HOST "iot-project-ebf71-default-rtdb.asia-southeast1.firebasedatabase.app"

// MQTT
#define MQTT_SERVER "broker.hivemq.com"
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

String pushTopic = "";
String subTopic = "";

void checkMqttConnection();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void pushOnOffMessage(bool isOn);

FirebaseData firebaseData;
FirebaseJson firebaseJson;

String deviceName = "QuyenLN";
String deviceRoomId = "1";
const int deviceType = 0; // relay 

// dht11 
void getDhtInfo();

// relay

void turnRelayOff()
{
  digitalWrite(RELAYPIN, HIGH);
  Serial1.print("Relay : ");
  Serial.println(digitalRead(RELAYPIN));
  pushOnOffMessage(false);
}

void turnRelayOn()
{
  digitalWrite(RELAYPIN, LOW);
  Serial.print("Relay : ");
  Serial.println(digitalRead(RELAYPIN));
  pushOnOffMessage(true);
}

// webserver
void setupWifi();
void setupDeviceOnFirebase();
void setupPinModeForDevices();

void updateInfoDeviceToFirebase(String mac, String ip, int type, String name, String roomId, String info);

// get sensor power
void getPowerUsed();

// button switch
void onSwitchPressed();

void setup()
{
  Serial.begin(9600);
  // set up wifi network
  setupWifi();
  // set up for firebase database
  setupDeviceOnFirebase();
  // set up for led
  setupPinModeForDevices();
  // dht11
  dht.begin();
  // mqtt
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);
}

void loop()
{
 checkMqttConnection();
  getDhtInfo();
  onSwitchPressed();
  getPowerUsed();
  delay(2000);
}

void setupWifi()
{
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.resetSettings();
  bool res;
  String deviceMacAddr = WiFi.macAddress();
  Serial.println(deviceMacAddr);
  char wifiName[128];
  deviceMacAddr.toCharArray(wifiName, sizeof(wifiName) / sizeof(char), 0);
  res = wm.autoConnect(wifiName, "123");
  if (!res)
  {
    Serial.println("Can not connected to wifi");
//     ESP.restart();
  }
  else
  {
    setupDeviceOnFirebase();
    Serial.print("Connect to WIFI: ");
    Serial.print(WiFi.localIP());
    String deviceIP = WiFi.localIP().toString();
    String deviceMacAddr = WiFi.macAddress();
    pushTopic += deviceMacAddr + "/" + "device_info";
    subTopic += deviceMacAddr;
    String deviceInfo = "off";
    updateInfoDeviceToFirebase(deviceMacAddr, deviceIP, deviceType, deviceName, deviceRoomId, deviceInfo);
  }
}

void setupDeviceOnFirebase()
{
  Firebase.begin(FIREBASE_HOST, FIREBASE_KEY);
}

void setupPinModeForDevices()
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAYPIN, OUTPUT);
  pinMode(sensorPower, INPUT);
  pinMode(SWITCH_PIN, INPUT);
}

void updateInfoDeviceToFirebase(String mac, String ip, int type, String name, String roomId, String info)
{
    FirebaseJson json;
    json.add("device_id", mac);
    json.add("device_ip_addr", ip);
    json.add("device_type", type);
    json.add("device_name", name);
    json.add("device_room_id", roomId);
    json.add("device_info", info);
    Firebase.setJSON(firebaseData, "/device/" + mac, json);
}

void getDhtInfo()
{
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t))
  {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  String topicTemp = "device_info/temperature";
  String topicHum = "device_info/humidity";
  const char *pushTemp = topicTemp.c_str();
  const char *pushHum = topicHum.c_str();
  String temp = String(t);
  const char* temperature = temp.c_str();
  String hum = String(h);
  const char* humidity = hum.c_str();
  client.publish(pushTemp, temperature);
  client.publish(pushHum, humidity);
  
}

void getPowerUsed()
{
  int sensorSignal = analogRead(sensorPower);
  float u = (sensorSignal / 1024.0) * 5;
  float i = ((u - 2.5) / 0.185);
  Serial.print("analogRead = ");
  Serial.println(analogRead(sensorPower));
  Serial.print("Intensity = ");
  Serial.print(i, 4); 
  Serial.println(" A");
  String powerTopic = "device_info/power";
  const char *push = powerTopic.c_str();
  String power = String(u * i);
  const char* powerStr = power.c_str();
  client.publish(push, powerStr);
}

void onSwitchPressed()
{
  Serial.print("switch = ");
  Serial.println(digitalRead(SWITCH_PIN));
}

void checkMqttConnection()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
}

void callback(char* topic, byte* payload, unsigned int length) {
  char *cnt = (char *)malloc((length + 1) * sizeof(char));
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    cnt[i] = (char)payload[i];
  }
  cnt[length] = '\0';
  String message(cnt);
  Serial.println(message);
  // Switch on the LED if an 1 was received as first character
  if (message.equals("ON") == true || message.equals("on") == true) {
    turnRelayOn();
  } else {
    turnRelayOff();
  }
  free(cnt);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      const char * sub = subTopic.c_str();
      client.subscribe(sub);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void pushOnOffMessage(bool isOn)
{
  const char *push = pushTopic.c_str();
  if(isOn)
  {
    client.publish(push, "on");
  }
  else
  {
    client.publish(push, "off");
  }
}