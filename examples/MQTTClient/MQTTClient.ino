#include <Arduino.h>
#include <WiFi.h>
#include <MycilaMQTT.h>

#define KEY_DEBUG_ENABLE "debug_enable"
#define KEY_WIFI_SSID "wifi_ssid"
#define KEY_WIFI_PWD "wifi_pwd"

const Mycila::MQTTConfig Mycila::MQTTClass::getConfig() {
  return {
    "192.168.125.90", // server
    1883,             // port
    false,            // ssl
    "homeassistant",  // user
    "",               // pass
    "my-app-1234",    // client id
    "my-app/status",  // will topic
    60,               // keep alive
  };
}

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

  Mycila::MQTT.onConnect([]() {
    ESP_LOGI("APP", "MQTT connected");
  });

  Mycila::MQTT.subscribe("my-app/value/set", [](const String& topic, const String& payload) {
    ESP_LOGI("APP", "MQTT message received: %s -> %s", topic.c_str(), payload.c_str());
  });

  Mycila::MQTT.begin();
}

void loop() {
  Mycila::MQTT.publish("my-app/value", "Hello World!");
  Mycila::MQTT.publish("my-app/value/set", "Hello you!");
  delay(2000);
}
