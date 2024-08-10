# MycilaMQTT

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Continuous Integration](https://github.com/mathieucarbou/MycilaMQTT/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaMQTT/actions/workflows/ci.yml)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mathieucarbou/library/MycilaMQTT.svg)](https://registry.platformio.org/libraries/mathieucarbou/MycilaMQTT)

A simple and efficient MQTT helper for Arduino / ESP32 based on Espressif MQTT Client

- Automatic reconnect
- Automatic resubscribe
- Dead simple configuration which allows easier integration with a configuration system and quick reload (`end()` and `begin()`) of the client
- automatic management of will topic
- Arduino 2 (ESP-IDF 4) Support
- Arduino 3 (ESP-IDF 5.1) Support
- SSL/TLS support (server certificates and CA bundles)
- Async mode (blocking mode or non-blocking mode when publishing)
- Does not support message fragmentation - reassembling: make sure to set buffer size accordingly (`MYCILA_MQTT_BUFFER_SIZE`)
- Retain support

## Usage

```cpp
  Mycila::MQTT::Config config;
  config.server = "test.mosquitto.org";
  config.port = 1884;
  config.username = "rw";
  config.password = "readwrite";
  config.clientId = "my-app-1234";
  config.willTopic = "foo/status";

  mqtt.onConnect([]() {
    Serial.println("MQTT connected");
  });

  mqtt.subscribe("my-app/value/set", [](const String& topic, const String& payload) {
    Serial.printf("MQTT message received: %s -> %s\r\n", topic.c_str(), payload.c_str());
  });

  mqtt.begin(config);
```

```c++
  mqtt.publish("my-app/value", "Hello World!");
```

## SSL / TLS

Please see the examples to see how to use server certificates or CA certificate bundles.

# Alternatives

[PsychicMqttClient](https://github.com/theelims/PsychicMqttClient) is also an MQTT library based on ESP-IDF written by [@theelims](https://github.com/theelims) that you might want to consider, which is more feature rich and has better support for CA bundles.

