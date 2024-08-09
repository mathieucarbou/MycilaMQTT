# MycilaMQTT

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Continuous Integration](https://github.com/mathieucarbou/MycilaMQTT/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaMQTT/actions/workflows/ci.yml)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mathieucarbou/library/MycilaMQTT.svg)](https://registry.platformio.org/libraries/mathieucarbou/MycilaMQTT)

A simple and efficient MQTT helper for Arduino / ESP32 based on Espressif MQTT Client

- Automatic reconnect
- Automatic resubscribe
- Dead simple configuration which allows easier integration with a configuration system and quick reload (`end()` and `begin()`) of the client
- automatic management of will topic
- Arduino 3 (ESP-IDF 5.1) Support

## Usage

```cpp
  mqtt.onConnect([]() {
    Serial.println("MQTT connected");
  });

  mqtt.subscribe("my-app/value/set", [](const String& topic, const String& payload) {
    Serial.printf("MQTT message received: %s -> %s\r\n", topic.c_str(), payload.c_str());
  });

  mqtt.begin();
```

```c++
  mqtt.publish("my-app/value", "Hello World!");
```

# Alternatives

[PsychicMqttClient](https://github.com/theelims/PsychicMqttClient) is also an MQTT library based on ESP-IDF written by [@theelims](https://github.com/theelims) that you might want to consider, which is more feature rich and has better support for CA bundles.

