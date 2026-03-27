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

  net.setInsecure();                     // Zoho TLS
  client.setServer(ZOHO_MQTT_BROKER, 8883);

  Serial.print("Connecting MQTT");
  while (!client.connected()) {
    if (client.connect(ZOHO_CLIENT_ID,
                       ZOHO_USERNAME,
                       ZOHO_AUTH_TOKEN)) {
      Serial.println(" Connected");
    } else {
      Serial.print(".");
      delay(1000);
    }
  }
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

  /* ---------- RFID ---------- */
  if (msg.startsWith("RFID,")) {
    StaticJsonDocument<256> in;
    if (deserializeJson(in, msg.substring(5))) return;

    unified["student_name"]   = in["name"];
    unified["student_uid"]    = in["uid"];
    unified["student_status"] = in["status"];
    unified["students_count"] = in["inside"];
    unified["alert_state"]    = "NORMAL";

    publishUnified();
  }

  /* ---------- ALERT ---------- */
  else if (msg.startsWith("ALERT,")) {
    StaticJsonDocument<256> in;
    if (deserializeJson(in, msg.substring(6))) return;

    if (strcmp(in["type"], "OVER_CAPACITY") == 0) {
      unified["students_count"] = in["inside"];
      unified["alert_state"]    = "OVER_CAPACITY";
    }
    else {   // CLEARED
      unified["alert_state"] = "CLEARED";
    }

    publishUnified();
  }
}

/* ========== SETUP ========== */
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  /* Init JSON defaults */
  unified["student_name"]   = "";
  unified["student_uid"]    = "";
  unified["student_status"] = "";
  unified["students_count"] = 0;
  unified["alert_state"]    = "NORMAL";

  connectWiFi();
  connectMQTT();

  Serial.println("ESP32 ZOHO MQTT GATEWAY READY");
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
