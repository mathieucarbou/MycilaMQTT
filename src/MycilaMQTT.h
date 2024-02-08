// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou and others
 */
#pragma once

#include <WString.h>
#include <espMqttClient.h>
#include <functional>
#include <vector>

#define MYCILA_MQTT_VERSION "1.2.1"
#define MYCILA_MQTT_VERSION_MAJOR 1
#define MYCILA_MQTT_VERSION_MINOR 2
#define MYCILA_MQTT_VERSION_REVISION 1

#ifndef MYCILA_MQTT_RECONNECT_INTERVAL
#define MYCILA_MQTT_RECONNECT_INTERVAL 5
#endif

#ifndef MYCILA_MQTT_WILL_TOPIC
#define MYCILA_MQTT_WILL_TOPIC "/status"
#endif

#ifndef MYCILA_MQTT_CLEAN_SESSION
#define MYCILA_MQTT_CLEAN_SESSION false
#endif

namespace Mycila {
  enum class MQTTState {
    // CONNECTED -> DISABLED
    // DISCONNECTED -> DISABLED
    // PUBLISHING -> DISABLED
    MQTT_DISABLED,
    // DISABLED -> CONNECTING
    MQTT_CONNECTING,
    // CONNECTING -> CONNECTED
    MQTT_CONNECTED,
    // CONNECTED -> PUBLISHING
    MQTT_PUBLISHING,
    // CONNECTED -> DISCONNECTED
    // PUBLISHING -> DISCONNECTED
    MQTT_DISCONNECTED,
  };

  typedef std::function<void(const String& topic, const String& payload)> MQTTMessageCallback;
  typedef std::function<void()> MQTTConnectedCallback;

  typedef struct
  {
      String topic;
      MQTTMessageCallback callback;
  } MQTTMessageListener;

  typedef struct {
      bool enabled;
      String server;
      uint16_t port;
      bool secured;
      String username;
      String password;
      String clientId;
      String baseTopic;
      String willTopic;
  } MQTTConfig;

  class MQTTClass {
    public:
      static const MQTTConfig getConfig();

    public:
      ~MQTTClass() { end(); }

      void begin();
      void loop();
      void end();

      void subscribe(const String& topic, MQTTMessageCallback callback);
      void unsubscribe(const String& topic);
      void onConnect(MQTTConnectedCallback callback) { _onConnect = callback; }

      bool publish(const char* topic, const char* payload, bool retain = false);
      inline bool publish(const String& topic, const String& payload, bool retain = false) {
        return publish(topic.c_str(), payload.c_str(), retain);
      }
      inline bool publish(const char* topic, const String& payload, bool retain = false) {
        return publish(topic, payload.c_str(), retain);
      }

      bool isEnabled() { return _state != MQTTState::MQTT_DISABLED; }
      bool isConnected() { return isEnabled() && _mqttClient->connected(); }

    private:
      MqttClient* _mqttClient = nullptr;
      MQTTState _state = MQTTState::MQTT_DISABLED;
      uint32_t _lastReconnectTry = 0;
      MQTTConnectedCallback _onConnect = nullptr;
      std::vector<MQTTMessageListener> _listeners;
      MQTTConfig _config;

    private:
      void _connect();
      void _onMqttConnect(bool sessionPresent);
      void _onMqttDisconnect(espMqttClientTypes::DisconnectReason reason);
      void _onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total);
      bool _topicMatches(const char* subscribed, const char* topic);
  };

  extern MQTTClass MQTT;
} // namespace Mycila
