#include <WiFi.h>
#include <PubSubClient.h>
#include "Audio.h"
#include "SD.h"
#include "FS.h"

// ---- SD & Audio pins ----
#define SD_CS    21
#define SD_SCK   7
#define SD_MISO  8
#define SD_MOSI  9

#define I2S_BCLK D3
#define I2S_LRC  D2
#define I2S_DOUT D4

Audio audio;

// ---- Wi-Fi & MQTT ----
const char* ssid       = "mushbrain";
const char* password   = "";
const char* mqtt_server= "mqtt.eclipseprojects.io";
const int   mqtt_port  = 1883;
const char* mqtt_topic = "esp32";

WiFiClient   espClient;
PubSubClient client(espClient);

bool playAudio   = false;
static bool wasRunning = false;
String audioFileName;

void setup() {
  Serial.begin(115200);
  delay(100);

  // --- init SD ---
  pinMode(SD_CS, OUTPUT);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  Serial.print("Init SD… ");
  if (!SD.begin(SD_CS)) {
    Serial.println("FAIL");
    while (1) delay(500);
  }
  Serial.println("OK");

  // list files on /
  Serial.println("Files on /:");
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    Serial.print("  ");
    Serial.println(entry.name());
    entry.close();
  }
  root.close();

  // --- configure I2S & volume ---
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(21);  // max

  // --- Wi-Fi ---
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");

  // --- MQTT ---
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();
}

void loop() {
  // keep MQTT alive
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // if we're supposed to be playing…
  if (playAudio) {
    bool running = audio.isRunning();

    if (running) {
      if (!wasRunning) {
        Serial.println("▶️ Started playback");
        wasRunning = true;
      }
      audio.loop();
    }
    else if (wasRunning) {
      Serial.println("▶️ Playback finished");
      wasRunning  = false;
      playAudio  = false;   
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.printf("Received message on topic %s: %s\n", topic, message.c_str());

  String file    = message + ".mp3"; 
  String path    = "/" + file; 
  // Map each command to its MP3 file:
  if (! SD.exists(path)) {
    Serial.printf("Audio file not found: %s\n", path.c_str());
    return;
  }

  audioFileName = file;
  client.publish(mqtt_topic, "true");

  // trigger playback of the selected file
  playAudio   = true;
  wasRunning  = false;
  if (audio.connecttoFS(SD, audioFileName.c_str())) {
    Serial.print("Audio file opened OK: ");
    Serial.println(audioFileName);
  } else {
    Serial.print("Audio file FAIL: ");
    Serial.println(audioFileName);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "esp32s3_";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" — retrying in 5 s");
      delay(5000);
    }
  }
}
