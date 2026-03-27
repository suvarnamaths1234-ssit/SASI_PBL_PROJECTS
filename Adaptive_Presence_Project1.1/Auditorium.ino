#define MQTT_MAX_PACKET_SIZE 512


#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

/* UART with LPC1768 */
#define RXD2 16
#define TXD2 17

WiFiClientSecure net;
PubSubClient client(net);

String rxLine = "";

/* ========== WIFI ========== */
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

/* ========== MQTT ========== */
void connectMQTT() {
  if (client.connected()) return;

  net.setInsecure();
  client.setServer(ZOHO_MQTT_BROKER, 8883);

  Serial.print("Connecting MQTT");
  while (!client.connected()) {
    client.connect(ZOHO_CLIENT_ID, ZOHO_USERNAME, ZOHO_AUTH_TOKEN);
    delay(1000);
    Serial.print(".");
  }
  Serial.println(" Connected");
}

/* ========== UNIFIED JSON STORAGE ========== */
StaticJsonDocument<512> unified;

/* ========== PUBLISH JSON ========== */
void publishUnified() {
  char payload[512];
  serializeJson(unified, payload);
  client.publish(ZOHO_PUB_TOPIC, payload);
  Serial.println(payload);
}

/* ========== PROCESS LPC MESSAGE ========== */
void processLPC(String msg) {

  /* ---------- ENV (Environmental Data) ---------- */
  if (msg.startsWith("ENV,")) {
    StaticJsonDocument<256> in;
    deserializeJson(in, msg.substring(4));

    unified["auditorium_temperature"] = in["temp"];
    unified["auditorium_humidity"]    = in["hum"];
    unified["air_quality"]            = in["air"];
    unified["audience_count"]         = in["inside"];
    unified["hall_status"]            = "MONITORING";

    publishUnified();
  }

  /* ---------- ENTRY (Someone Entered) ---------- */
  else if (msg.startsWith("ENTRY,")) {
    StaticJsonDocument<256> in;
    deserializeJson(in, msg.substring(6));

    unified["audience_count"] = in["count"];
    unified["last_event"]     = "ENTRY";
    unified["hall_status"]    = "ACTIVE";

    publishUnified();
  }

  /* ---------- EXIT (Someone Left) ---------- */
  else if (msg.startsWith("EXIT,")) {
    StaticJsonDocument<256> in;
    deserializeJson(in, msg.substring(5));

    unified["audience_count"] = in["count"];
    unified["last_event"]     = "EXIT";
    unified["hall_status"]    = "ACTIVE";

    publishUnified();
  }

  /* ---------- RESET (System Reset) ---------- */
  else if (msg.startsWith("RESET,")) {
    StaticJsonDocument<256> in;
    deserializeJson(in, msg.substring(6));

    unified["audience_count"]         = in["count"];
    unified["last_event"]             = "RESET";
    unified["hall_status"]            = "RESET";
    unified["auditorium_temperature"] = 0;
    unified["auditorium_humidity"]    = 0;
    unified["air_quality"]            = 0;

    publishUnified();
  }

  /* ---------- CAPACITY (Hall Full Alert) ---------- */
  else if (msg.startsWith("CAPACITY,")) {
    StaticJsonDocument<256> in;
    deserializeJson(in, msg.substring(9));

    unified["audience_count"] = in["count"];
    unified["last_event"]     = "CAPACITY_FULL";
    unified["hall_status"]    = "FULL";
    unified["alert_message"]  = "HALL AT MAXIMUM CAPACITY";

    publishUnified();
  }
}

/* ========== SETUP ========== */
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  /* Init JSON with defaults */
  unified["audience_count"]         = 0;
  unified["auditorium_temperature"] = 0;
  unified["auditorium_humidity"]    = 0;
  unified["air_quality"]            = 0;
  unified["hall_status"]            = "IDLE";
  unified["last_event"]             = "NONE";
  unified["alert_message"]          = "";

  connectWiFi();
  connectMQTT();

  Serial.println("ESP32 AUDITORIUM MQTT GATEWAY READY");
}

/* ========== LOOP ========== */
void loop() {
  if (!client.connected()) connectMQTT();
  client.loop();

  while (Serial2.available()) {
    char c = Serial2.read();

    if (c == '\n') {
      rxLine.trim();
      if (rxLine.length() > 0) {
        Serial.print("RX: ");
        Serial.println(rxLine);
        processLPC(rxLine);
      }
      rxLine = "";
    }
    else if (c != '\r') {
      rxLine += c;
    }
  }
}



