// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou and others
 */
#pragma once

#include <mqtt_client.h>

#include <WString.h>
#include <esp32-hal-log.h>
#include <functional>
#include <vector>

#define MYCILA_MQTT_VERSION "2.0.0"
#define MYCILA_MQTT_VERSION_MAJOR 2
#define MYCILA_MQTT_VERSION_MINOR 0
#define MYCILA_MQTT_VERSION_REVISION 0

#ifndef MYCILA_MQTT_RECONNECT_INTERVAL
#define MYCILA_MQTT_RECONNECT_INTERVAL 5
#endif

#ifndef MYCILA_MQTT_CLEAN_SESSION
#define MYCILA_MQTT_CLEAN_SESSION false
#endif

#ifndef MYCILA_MQTT_TASK_PRIORITY
#define MYCILA_MQTT_TASK_PRIORITY 5
#endif

#ifndef MYCILA_MQTT_STACK_SIZE
#define MYCILA_MQTT_STACK_SIZE 6144
#endif

#ifndef MYCILA_MQTT_BUFFER_SIZE
#define MYCILA_MQTT_BUFFER_SIZE 1024
#endif

#ifndef MYCILA_MQTT_NETWORK_TIMEOUT
#define MYCILA_MQTT_NETWORK_TIMEOUT 10
#endif

#ifndef MYCILA_MQTT_RETRANSMIT_TIMEOUT
#define MYCILA_MQTT_RETRANSMIT_TIMEOUT 1
#endif

#define MYCILA_MQTT_TASK_NAME "mqtt_task"

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
      String server;
      uint16_t port;
      bool secured;
      String username;
      String password;
      String clientId;
      String willTopic;
      uint16_t keepAlive;
  } MQTTConfig;

  class MQTTClass {
    public:
      static const MQTTConfig getConfig();

    public:
      ~MQTTClass() { end(); }

      void begin();
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
      bool isConnected() { return _state == MQTTState::MQTT_CONNECTED; }
      const char* getLastError() { return _lastError; }

    private:
      esp_mqtt_client_handle_t _mqttClient = nullptr;
      MQTTState _state = MQTTState::MQTT_DISABLED;
      MQTTConnectedCallback _onConnect = nullptr;
      std::vector<MQTTMessageListener> _listeners;
      MQTTConfig _config;
      const char* _lastError = nullptr;

    private:
      static void _mqttEventHandler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
      static bool _topicMatches(const char* subscribed, const char* topic);
  };

  extern MQTTClass MQTT;
} // namespace Mycila
