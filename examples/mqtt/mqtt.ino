#include <Arduino.h>
#include <MycilaMQTT.h>
#include <WiFi.h>

Mycila::MQTT mqtt;

String baseTopic = "CC74C9F97C8D";

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  WiFi.mode(WIFI_STA);
  WiFi.begin("IoT");

  while (WiFi.status() != WL_CONNECTED) {
    ESP_LOGI("APP", "Connecting to WiFi...");
    delay(1000);
  }

  mqtt.onConnect([]() {
    ESP_LOGI("APP", "MQTT connected");
  });

  mqtt.subscribe(baseTopic + "/value/set", [](const String& topic, const String& payload) {
    ESP_LOGI("APP", "MQTT message received: %s -> %s", topic.c_str(), payload.c_str());
  });

  Mycila::MQTT::Config config;
  config.server = "test.mosquitto.org";
  config.port = 1884;
  config.username = "rw";
  config.password = "readwrite";
  config.clientId = "my-app-1234";
  config.willTopic = baseTopic + "/status";

  // mqtt.setAsync(true);
  mqtt.begin(config);
}

void loop() {
  mqtt.publish(baseTopic + "/value", String(millis()));
  delay(500);
}
