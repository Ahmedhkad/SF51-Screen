#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <RCSwitch.h>
#include "settings.h" //GPIO defines, NodeMCU good-pin table
#include "secret.h"   //Wifi and mqtt server info

RCSwitch mySwitch = RCSwitch();

const char *ssid = ssidWifi;       // defined on secret.h
const char *password = passWifi;   // defined on secret.h
const char *mqtt_server = mqttURL; // defined on secret.h
const char *deviceName = mqttClient;

WiFiClient espClient;
PubSubClient client(espClient);

int motorEnableState;
int motorDirState;

StaticJsonDocument<100> doc;
StaticJsonDocument<300> psuupdater;
StaticJsonDocument<300> updater;

int device;
int valuejson;
int datajson;

int lastCount = 0;
int count = 0;

unsigned long WifiDelayMillis = 0;
const long WifiDelayInterval = 5000; // interval to check wifi and mqtt

unsigned long previousMillis = 0;
unsigned long previousMillis2 = 0;
unsigned long secondMeter = 0;
unsigned long PSUDelayInterval = 0; // interval to check wifi and mqtt

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.hostname(deviceName); // DHCP Hostname (useful for finding device for static lease)
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
  {
    Serial.println("STA Failed to configure");
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(300);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Test if parsing succeeds.
  DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }
  // Print the values to data types
  device = doc["device"].as<unsigned int>();
  valuejson = doc["value"].as<unsigned int>();
  datajson = doc["data"].as<unsigned int>();
  Serial.print("device: ");
  Serial.print(device);
  Serial.print(" valuejson: ");
  Serial.println(valuejson);

  switch (device)
  {
  case 1:
    if (valuejson == 1)
    {
      mySwitch.send(3259518, 24);
      delay(100);
      mySwitch.send(3259518, 24);
      client.publish("rfKitchen", "ON");
    }
    else if (valuejson == 0)
    {
      mySwitch.send(2459500, 24);
      delay(100);
      mySwitch.send(2459500, 24);
      client.publish("rfKitchen", "OFF");
    }
    break;

  case 2:
    if (valuejson == 1)
    {
      mySwitch.send(3157410, 24);
      delay(100);
      mySwitch.send(3157410, 24);
      client.publish("rfBathroom", "ON");
    }
    else if (valuejson == 0)
    {
      mySwitch.send(2359403, 24);
      delay(100);
      mySwitch.send(2359403, 24);
      client.publish("rfBathroom", "OFF");
    }
    break;

  case 3:
    if (valuejson == 1) // Rolling Screen UP
    {

      digitalWrite(motorBIn1, HIGH);
      digitalWrite(motorBIn2, LOW);
      client.publish("rollerUP", "OFF");
      client.publish("rollerDown", "ON");
    }
    else if (valuejson == 2) // Rolling Screen UP
    {

      digitalWrite(motorBIn1, HIGH);
      digitalWrite(motorBIn2, HIGH);
      client.publish("rollerUP", "ON");
      client.publish("rollerDown", "OFF");
    }
    else if (valuejson == 0)
    {
      digitalWrite(motorBIn1, LOW);
      digitalWrite(motorBIn2, LOW);
      client.publish("rollerUP", "OFF");
      client.publish("rollerDown", "OFF");
    }
    break;

  case 5:
    if (valuejson == 1) // POWER Supply
    {
      digitalWrite(PSU, HIGH);
      client.publish("PSU", "ON");
    }
    else if (valuejson == 2) // Add times on
    {
      PSUDelayInterval = PSUDelayInterval + (datajson * 1000); // in seconds
      previousMillis = millis();
      digitalWrite(PSU, HIGH);
      client.publish("PSU", "ON");
    }
    else if (valuejson == 3)
    {
      PSUDelayInterval = 1000;
      digitalWrite(PSU, LOW);
      client.publish("PSU", "OFF");
    }
    break;

  case 6:
    if (valuejson == 1) // Rolling Speaker Left
    {

      analogWrite(motorAIn1, 190);
      digitalWrite(motorAIn2, LOW);
      client.publish("SpeakerRight", "OFF");
      client.publish("SpeakerLeft", "ON");
    }
    else if (valuejson == 2) // Rolling Speaker Right
    {

      analogWrite(motorAIn1, 190);
      digitalWrite(motorAIn2, HIGH);
      client.publish("SpeakerRight", "ON");
      client.publish("SpeakerLeft", "OFF");
    }
    else if (valuejson == 0)
    {
      digitalWrite(motorAIn1, LOW);
      digitalWrite(motorAIn2, LOW);
      client.publish("SpeakerRight", "OFF");
      client.publish("SpeakerLeft", "OFF");
    }
    break;

  default:
    Serial.print("Err device in case-switch invalid.");
    break;
  }
}

void reconnect()
{
  // Loop until we're reconnected
  if (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    uint32_t timestamp = millis() / 1000;
    char clientid[23];
    snprintf(clientid, 23, mqttClient "%02X", timestamp);
    Serial.print("Client ID: ");
    Serial.println(clientid);

    if (client.connect(clientid, mqttName, mqttPASS, mqttWillTopic, 0, true, "offline"))
    {
      Serial.println("Connected");
      // Once connected, publish an announcement...
      client.publish(mqttWillTopic, "online", true);
      // ... and resubscribe
      client.subscribe(subscribeTopic);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      count = count + 1;
    }
  }
}

void setup()
{
  pinMode(rf, OUTPUT);
  pinMode(PSU, OUTPUT);
  pinMode(motorBIn1, OUTPUT);
  pinMode(motorBIn2, OUTPUT);
  pinMode(motorAIn1, OUTPUT);
  pinMode(motorAIn2, OUTPUT);

  Serial.begin(115200); // debug print on Serial Monitor

  mySwitch.enableTransmit(rf);
  setup_wifi();
  client.setServer(mqtt_server, mqttPORT);
  client.setCallback(callback);

  ArduinoOTA.setHostname(mqttClient);
  // ArduinoOTA.setPort(otaPort);
  // ArduinoOTA.setPassword(otaPass);
  ArduinoOTA.begin();
}

void loop()
{
  ArduinoOTA.handle();
  unsigned long currentMillis = millis();

  if (currentMillis - WifiDelayMillis >= WifiDelayInterval)
  {
    WifiDelayMillis = currentMillis;
    if (!client.connected())
    {
      Serial.println("reconnecting ...");
      reconnect();
    }
    else if (lastCount != count)
    {
      char buffer[200];
      updater["Disconnected"] = count;
      serializeJson(updater, buffer);
      client.publish(mqttDisconnectTopic, buffer, true);
      lastCount = count;
    }
  }

  // Power UP Power Supply (PSU) for exact time and then turm it off
  if (currentMillis - previousMillis <= PSUDelayInterval)
  {
    digitalWrite(PSU, HIGH);
    previousMillis2 = currentMillis;
  }
  else if (currentMillis - previousMillis2 <= 120)
  {
    digitalWrite(PSU, LOW);
    client.publish("PSU", "OFF");
    delay(100);
    PSUDelayInterval = 0;
  }

  client.loop();
}