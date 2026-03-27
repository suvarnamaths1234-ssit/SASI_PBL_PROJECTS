#define MQTT_MAX_PACKET_SIZE 512

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

/* UART with LPC1768 */   //// UART0 (USB UART)
//#define LPC_UART Serial

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

  /* ---------- RFID ---------- */
  if (msg.startsWith("RFID,")) {
    StaticJsonDocument<256> in;
    deserializeJson(in, msg.substring(5));

    unified["worker_name"]   = in["name"];
    unified["worker_id"]     = in["uid"];
    unified["worker_status"] = in["status"];
    unified["workers_count"] = in["inside"];
    unified["emergency_msg"] = "NORMAL";

    publishUnified();
  }

  /* ---------- ENV ---------- */
  else if (msg.startsWith("ENV,")) {
    StaticJsonDocument<256> in;
    deserializeJson(in, msg.substring(4));

    unified["temperature_mine"]   = in["temp"];
    unified["humidity_mine"]      = in["hum"];
    unified["air"]           = in["air"];
    unified["workers_count"] = in["inside"];
    unified["emergency_msg"] = "NORMAL";

    publishUnified();
  }

  /* ---------- ALERT ---------- */
  else if (msg.startsWith("ALERT,")) {
    StaticJsonDocument<256> in;
    deserializeJson(in, msg.substring(6));

    if (strcmp(in["type"], "EMERGENCY") == 0) {
      unified["temperature_mine"]   = in["temp"];
      unified["humidity_mine"]      = in["hum"];
      unified["air"]           = in["air"];
      unified["workers_count"] = in["inside"];
      unified["emergency_msg"] = "EMERGENCY";
    }
    else {
      unified["emergency_msg"] = "CLEARED";
    }

    publishUnified();
  }
}

/* ========== SETUP ========== */
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);                   //void setup() {
                                                         //                       Serial.begin(9600);   // UART0 for LPC1768

  connectWiFi();
  connectMQTT();
}


  /* Init JSON with defaults */
  unified["worker_name"]   = "";
  unified["worker_id"]     = "";
  unified["worker_status"] = "";
  unified["workers_count"] = 0;
  unified["temperature_mine"]   = 0;
  unified["humidity_mine"]      = 0;
  unified["air"]           = 0;
  unified["emergency_msg"] = "NORMAL";

  connectWiFi();
  connectMQTT();

  Serial.println("ESP32 MQTT GATEWAY READY");
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
