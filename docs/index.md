# MycilaMQTT

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Continuous Integration](https://github.com/mathieucarbou/MycilaMQTT/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaMQTT/actions/workflows/ci.yml)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mathieucarbou/library/MycilaMQTT.svg)](https://registry.platformio.org/libraries/mathieucarbou/MycilaMQTT)

A simple and efficient MQTT helper for Arduino / ESP32 based on bertmelis/espMqttClient

- Based on the very good and stable async MQTT client [bertmelis/espMqttClient](https://github.com/bertmelis/espMqttClient/)
- Automatic reconnect
- Automatic resubscribe
- Dead simple configuration which allows easier integration with a configuration system and quick reload (`end()` and `begin()`) of the client
- `onConnect()` callback runs in the `loop()` task and not the async `mqttClient` task
- automatic management of will topic

## MQTT Implementation

`bertmelis/espMqttClient @^1.6.0`

## Usage

```cpp
  Mycila::MQTT.onConnect([]() {
    Serial.println("MQTT connected");
  });

  Mycila::MQTT.subscribe("my-app/value/set", [](const String& topic, const String& payload) {
    Serial.printf("MQTT message received: %s -> %s\r\n", topic.c_str(), payload.c_str());
  });

  Mycila::MQTT.begin();
```

```c++
  Mycila::MQTT.publish("my-app/value", "Hello World!");
```
