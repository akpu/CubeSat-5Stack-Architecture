#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

// ================= WIFI =================
const char* ssid = "Oppo";
const char* password = "hello1234@";

// ================= MQTT =================
const char* mqtt_server = "broker.hivemq.com";   // ✅ FIX 4
const char* topic = "ktu/kiran/balloon/telemetry";

// ================= LORA PINS =================
#define LORA_SS   5
#define LORA_RST  33
#define LORA_DIO0 35

WiFiClient espClient;
PubSubClient client(espClient);

// ================= WIFI =================
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(" connected");
}

// ================= MQTT =================
void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");
    
    if (client.connect("GroundStation")) {
      Serial.println(" connected");
    } else {
      Serial.print(" failed rc=");
      Serial.println(client.state());
      Serial.println("Retrying in 2 sec...");
      delay(2000);
    }
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  connectWiFi();

  client.setServer(mqtt_server, 1883);
  client.setBufferSize(1024);  // for large JSON

  SPI.begin(18, 34, 23, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed");
    while (1);
  }

  Serial.println("Ground Station Ready\n");
}

// ================= LOOP =================
void loop() {

  // ✅ Ensure WiFi stays connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost. Reconnecting...");
    connectWiFi();
  }

  // ✅ Ensure MQTT connected
  if (!client.connected()) {
    connectMQTT();
  }

  client.loop();

  int packetSize = LoRa.parsePacket();

  if (packetSize) {

    String received = "";

    while (LoRa.available()) {
      received += (char)LoRa.read();
    }

    Serial.println("\n================ FULL PACKET ================");
    Serial.println(received);
    Serial.println("=============================================");

    // ===== JSON CHECK =====
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, received);

    if (error) {
      Serial.println("JSON Parse Failed — forwarding raw anyway");
    } else {

      Serial.println("\nDecoded Values:");

      // GPS
      Serial.printf("Lat: %.6f\n", doc["lat"].as<float>());
      Serial.printf("Lng: %.6f\n", doc["lng"].as<float>());
      Serial.printf("GPS Alt: %.2f\n", doc["galt"].as<float>());
      Serial.printf("Sat: %d\n", doc["sat"].as<int>());

      // ACCEL
      Serial.printf("AX: %.2f AY: %.2f AZ: %.2f\n",
        doc["ax"].as<float>(),
        doc["ay"].as<float>(),
        doc["az"].as<float>());

      // GYRO
      Serial.printf("GX: %.2f GY: %.2f GZ: %.2f\n",
        doc["gx"].as<float>(),
        doc["gy"].as<float>(),
        doc["gz"].as<float>());

      // MAG
      Serial.printf("MX: %.2f MY: %.2f MZ: %.2f\n",
        doc["mx"].as<float>(),
        doc["my"].as<float>(),
        doc["mz"].as<float>());

      // ENV
      Serial.printf("Temp: %.2f Pressure: %.2f BaroAlt: %.2f\n",
        doc["t"].as<float>(),
        doc["p"].as<float>(),
        doc["balt"].as<float>());

      // POWER
      Serial.printf("Volt: %.2f Curr: %.2f Power: %.2f\n",
        doc["v"].as<float>(),
        doc["c"].as<float>(),
        doc["pw"].as<float>());

      // ORIENTATION
      Serial.printf("Roll: %.2f Pitch: %.2f Yaw: %.2f\n",
        doc["r"].as<float>(),
        doc["pi"].as<float>(),
        doc["y"].as<float>());
    }

    Serial.print("RSSI: ");
    Serial.println(LoRa.packetRssi());

    Serial.print("SNR: ");
    Serial.println(LoRa.packetSnr());

    // ===== PUBLISH =====
    if (client.publish(topic, received.c_str())) {
      Serial.println("Forwarded FULL packet to MQTT");
    } else {
      Serial.println("MQTT Publish FAILED");
    }
  }
}
