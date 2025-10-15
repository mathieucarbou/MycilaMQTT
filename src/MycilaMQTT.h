// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#pragma once

#include <mqtt_client.h>

#include <functional>
#include <list>
#include <string>

#define MYCILA_MQTT_VERSION          "6.1.1"
#define MYCILA_MQTT_VERSION_MAJOR    6
#define MYCILA_MQTT_VERSION_MINOR    1
#define MYCILA_MQTT_VERSION_REVISION 1

#ifndef MYCILA_MQTT_RECONNECT_INTERVAL
  #define MYCILA_MQTT_RECONNECT_INTERVAL 10
#endif

#ifndef MYCILA_MQTT_CLEAN_SESSION
  #define MYCILA_MQTT_CLEAN_SESSION false
#endif

#ifndef MYCILA_MQTT_TASK_PRIORITY
  #define MYCILA_MQTT_TASK_PRIORITY 5
#endif

#ifndef MYCILA_MQTT_STACK_SIZE
  #define MYCILA_MQTT_STACK_SIZE 4096
#endif

#ifndef MYCILA_MQTT_BUFFER_SIZE
  #define MYCILA_MQTT_BUFFER_SIZE 1024
#endif

#ifndef MYCILA_MQTT_NETWORK_TIMEOUT
  #define MYCILA_MQTT_NETWORK_TIMEOUT 10
#endif

#ifndef MYCILA_MQTT_RETRANSMIT_TIMEOUT
  #define MYCILA_MQTT_RETRANSMIT_TIMEOUT 0
#endif

#ifndef MYCILA_MQTT_OUTBOX_SIZE
  #define MYCILA_MQTT_OUTBOX_SIZE 0
#endif

#ifndef MYCILA_MQTT_KEEPALIVE
  #define MYCILA_MQTT_KEEPALIVE 60
#endif

#define MYCILA_MQTT_TASK_NAME "mqtt_task"

namespace Mycila {
  class MQTT {
    public:
      enum class State {
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

      typedef std::function<void(const std::string& topic, const std::string_view& payload)> MessageCallback;
      typedef std::function<void()> ConnectedCallback;

      typedef struct
      {
          std::string topic;
          MessageCallback callback;
      } MQTTMessageListener;

      typedef struct {
          std::string server;
          uint16_t port = 1883;
          bool secured = false;
          uint16_t keepAlive = MYCILA_MQTT_KEEPALIVE;
          std::string username;
          std::string password;
          std::string clientId;
          std::string willTopic;

          std::string serverCert;              // Server certificate (PEM format, loaded from File)
          const char* serverCertPtr = nullptr; // Server certificate pointer (PEM format)

          const uint8_t* certBundle = nullptr; // CA certificate bundle pointer
          size_t certBundleSize = 0;           // CA certificate bundle size
      } Config;

      ~MQTT() { end(); }

      void setConfigHook(std::function<void(esp_mqtt_client_config_t& cfg)> hook) { _configHook = hook; }

      void begin(Config config);
      void end();

      void setAsync(bool async) { _async = async; }
      bool isAsync() { return _async; }

      void subscribe(std::string topic, MessageCallback callback);
      void unsubscribe(const char* topic);

      bool publish(const char* topic, const std::string_view& payload, bool retain = false);

      void onConnect(ConnectedCallback callback) { _onConnect = callback; }

      bool isEnabled() { return _state != State::MQTT_DISABLED; }
      bool isConnected() { return _state == State::MQTT_CONNECTED; }
      const char* getLastError() { return _lastError; }

      static const esp_mqtt_client_config_t to_esp_mqtt_client_config(const Config& config);

    private:
      esp_mqtt_client_handle_t _mqttClient = nullptr;
      State _state = State::MQTT_DISABLED;
      ConnectedCallback _onConnect = nullptr;
      std::list<MQTTMessageListener> _listeners;
      Config _config;
      const char* _lastError = nullptr;
      bool _async = false;
      std::function<void(esp_mqtt_client_config_t& cfg)> _configHook = nullptr;

    private:
      static void _mqttEventHandler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
      static bool _topicMatches(const char* subscribed, const char* topic);
  };
} // namespace Mycila
