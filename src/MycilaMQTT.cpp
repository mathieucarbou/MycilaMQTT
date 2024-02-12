// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou and others
 */
#include <MycilaMQTT.h>

#include <functional>

#define TAG "MQTT"

void Mycila::MQTTClass::begin() {
  if (_state != MQTTState::MQTT_DISABLED)
    return;

  _config = Mycila::MQTT.getConfig();

  if (!_config.enabled)
    return;

  ESP_LOGI(TAG, "Enable MQTT...");

  if (_config.secured)
    _mqttClient = static_cast<MqttClient*>(new espMqttClientSecure(uxTaskPriorityGet(NULL), xPortGetCoreID()));
  else
    _mqttClient = static_cast<MqttClient*>(new espMqttClient(uxTaskPriorityGet(NULL), xPortGetCoreID()));

  _connect();
}

void Mycila::MQTTClass::end() {
  if (_state == MQTTState::MQTT_DISABLED)
    return;

  ESP_LOGI(TAG, "Disable MQTT...");

  _state = MQTTState::MQTT_DISABLED;
  _mqttClient->publish(_config.willTopic.c_str(), 0, true, "offline");
  _mqttClient->disconnect();

  delete _mqttClient;
  _mqttClient = nullptr;
  _lastReconnectTry = 0;
}

void Mycila::MQTTClass::loop() {
  if (_state == MQTTState::MQTT_DISCONNECTED && millis() - _lastReconnectTry >= MYCILA_MQTT_RECONNECT_INTERVAL * 1000) {
    _lastReconnectTry = millis();
    _connect();
    return;
  }

  if (_state == MQTTState::MQTT_CONNECTED) {
    if (_onConnect)
      _onConnect();
    _state = MQTTState::MQTT_PUBLISHING;
    return;
  }
}

bool Mycila::MQTTClass::publish(const char* topic, const char* payload, bool retain) {
  if (!isConnected())
    return false;
  return _mqttClient->publish(topic, 0, retain, payload);
}

void Mycila::MQTTClass::subscribe(const String& topic, MQTTMessageCallback callback) {
  _listeners.push_back({topic, callback});
  if (isConnected()) {
    ESP_LOGD(TAG, "Subscribing to: %s...", topic.c_str());
    _mqttClient->subscribe(topic.c_str(), 0);
  }
}

void Mycila::MQTTClass::unsubscribe(const String& topic) {
  ESP_LOGD(TAG, "Unsubscribing from: %s...", topic.c_str());
  _mqttClient->unsubscribe(topic.c_str());
  remove_if(_listeners.begin(), _listeners.end(), [&topic](const MQTTMessageListener& listener) {
    return listener.topic == topic;
  });
}

void Mycila::MQTTClass::_connect() {
  _state = MQTTState::MQTT_CONNECTING;

  if (_config.server.isEmpty() || _config.baseTopic.isEmpty() || _config.port <= 0) {
    ESP_LOGE(TAG, "MQTT disabled: Invalid server, port or base topic");
    return;
  }

  ESP_LOGI(TAG, "Connecting to MQTT server %s:%u...", _config.server.c_str(), _config.port);
  ESP_LOGD(TAG, "- Secured: %s", _config.secured ? "true" : "false");
  ESP_LOGD(TAG, "- Username: %s", _config.username.c_str());
  ESP_LOGD(TAG, "- Password: %s", _config.password.c_str());
  ESP_LOGD(TAG, "- ClientId: %s", _config.clientId.c_str());
  ESP_LOGD(TAG, "- Prefix: %s", _config.baseTopic.c_str());
  ESP_LOGD(TAG, "- Will: %s", _config.willTopic.c_str());
  ESP_LOGD(TAG, "- Clean Session: %s", MYCILA_MQTT_CLEAN_SESSION ? "true" : "false");

  if (_config.secured) {
    static_cast<espMqttClientSecure*>(_mqttClient)->setInsecure();
    static_cast<espMqttClientSecure*>(_mqttClient)->setServer(_config.server.c_str(), _config.port);
    static_cast<espMqttClientSecure*>(_mqttClient)->setCredentials(_config.username.c_str(), _config.password.c_str());
    static_cast<espMqttClientSecure*>(_mqttClient)->setWill(_config.willTopic.c_str(), 2, true, "offline");
    static_cast<espMqttClientSecure*>(_mqttClient)->setClientId(_config.clientId.c_str());
    static_cast<espMqttClientSecure*>(_mqttClient)->setCleanSession(MYCILA_MQTT_CLEAN_SESSION);
    static_cast<espMqttClientSecure*>(_mqttClient)->onConnect(std::bind(&Mycila::MQTTClass::_onMqttConnect, this, std::placeholders::_1));
    static_cast<espMqttClientSecure*>(_mqttClient)->onDisconnect(std::bind(&Mycila::MQTTClass::_onMqttDisconnect, this, std::placeholders::_1));
    static_cast<espMqttClientSecure*>(_mqttClient)->onMessage(std::bind(&Mycila::MQTTClass::_onMqttMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
  } else {
    static_cast<espMqttClient*>(_mqttClient)->setServer(_config.server.c_str(), _config.port);
    static_cast<espMqttClient*>(_mqttClient)->setCredentials(_config.username.c_str(), _config.password.c_str());
    static_cast<espMqttClient*>(_mqttClient)->setWill(_config.willTopic.c_str(), 2, true, "offline");
    static_cast<espMqttClient*>(_mqttClient)->setClientId(_config.clientId.c_str());
    static_cast<espMqttClient*>(_mqttClient)->setCleanSession(MYCILA_MQTT_CLEAN_SESSION);
    static_cast<espMqttClient*>(_mqttClient)->onConnect(std::bind(&Mycila::MQTTClass::_onMqttConnect, this, std::placeholders::_1));
    static_cast<espMqttClient*>(_mqttClient)->onDisconnect(std::bind(&Mycila::MQTTClass::_onMqttDisconnect, this, std::placeholders::_1));
    static_cast<espMqttClient*>(_mqttClient)->onMessage(std::bind(&Mycila::MQTTClass::_onMqttMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
  }

  _mqttClient->connect();
}

void Mycila::MQTTClass::_onMqttConnect(bool sessionPresent) {
  ESP_LOGD(TAG, "Connected to MQTT");
  _mqttClient->publish(_config.willTopic.c_str(), 0, true, "online");
  ESP_LOGD(TAG, "Subscribing to %u topics...", _listeners.size());
  for (auto& _listener : _listeners) {
    String t = _listener.topic;
    ESP_LOGD(TAG, "Subscribing to: %s", t.c_str());
    _mqttClient->subscribe(t.c_str(), 0);
  }
  _state = MQTTState::MQTT_CONNECTED;
}

void Mycila::MQTTClass::_onMqttDisconnect(espMqttClientTypes::DisconnectReason reason) {
  switch (reason) {
    case espMqttClientTypes::DisconnectReason::TCP_DISCONNECTED:
      _disconnectReason = "TCP disconnected";
      break;
    case espMqttClientTypes::DisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
      _disconnectReason = "Unacceptable protocol version";
      break;
    case espMqttClientTypes::DisconnectReason::MQTT_IDENTIFIER_REJECTED:
      _disconnectReason = "ID rejected";
      break;
    case espMqttClientTypes::DisconnectReason::MQTT_SERVER_UNAVAILABLE:
      _disconnectReason = "Server unavailable";
      break;
    case espMqttClientTypes::DisconnectReason::MQTT_MALFORMED_CREDENTIALS:
      _disconnectReason = "Malformed credentials";
      break;
    case espMqttClientTypes::DisconnectReason::MQTT_NOT_AUTHORIZED:
      _disconnectReason = "Not authorized";
      break;
    default:
      _disconnectReason = "Unknown error";
  }
  ESP_LOGW(TAG, "Disconnected from MQTT. Reason: %s", _disconnectReason);
  _state = MQTTState::MQTT_DISCONNECTED;
}

void Mycila::MQTTClass::_onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) {
  const String data = String(payload, len);
  ESP_LOGD(TAG, "Received message on topic %s: %s", topic, data.c_str());
  for (auto& listener : _listeners)
    if (_topicMatches(listener.topic.c_str(), topic))
      listener.callback(topic, data);
}

bool Mycila::MQTTClass::_topicMatches(const char* sub, const char* topic) {
  // ESP_LOGD(TAG, "Match: %s vs %s ?", sub, topic);
  size_t spos;

  if (!sub || !topic || sub[0] == 0 || topic[0] == 0)
    return false;

  if ((sub[0] == '$' && topic[0] != '$') || (topic[0] == '$' && sub[0] != '$'))
    return false;

  spos = 0;

  while (sub[0] != 0) {
    if (topic[0] == '+' || topic[0] == '#')
      return false;

    if (sub[0] != topic[0] || topic[0] == 0) { /* Check for wildcard matches */
      if (sub[0] == '+') {
        /* Check for bad "+foo" or "a/+foo" subscription */
        if (spos > 0 && sub[-1] != '/')
          return false;

        /* Check for bad "foo+" or "foo+/a" subscription */
        if (sub[1] != 0 && sub[1] != '/')
          return false;

        spos++;
        sub++;
        while (topic[0] != 0 && topic[0] != '/') {
          if (topic[0] == '+' || topic[0] == '#')
            return false;
          topic++;
        }
        if (topic[0] == 0 && sub[0] == 0)
          return true;
      } else if (sub[0] == '#') {
        /* Check for bad "foo#" subscription */
        if (spos > 0 && sub[-1] != '/')
          return false;

        /* Check for # not the final character of the sub, e.g. "#foo" */
        if (sub[1] != 0)
          return false;
        else {
          while (topic[0] != 0) {
            if (topic[0] == '+' || topic[0] == '#')
              return false;
            topic++;
          }
          return true;
        }
      } else {
        /* Check for e.g. foo/bar matching foo/+/# */
        if (topic[0] == 0 && spos > 0 && sub[-1] == '+' && sub[0] == '/' && sub[1] == '#')
          return true;

        /* There is no match at this point, but is the sub invalid? */
        while (sub[0] != 0) {
          if (sub[0] == '#' && sub[1] != 0)
            return false;
          spos++;
          sub++;
        }

        /* Valid input, but no match */
        return false;
      }
    } else {
      /* sub[spos] == topic[tpos] */
      if (topic[1] == 0) {
        /* Check for e.g. foo matching foo/# */
        if (sub[1] == '/' && sub[2] == '#' && sub[3] == 0)
          return true;
      }
      spos++;
      sub++;
      topic++;
      if (sub[0] == 0 && topic[0] == 0)
        return true;
      else if (topic[0] == 0 && sub[0] == '+' && sub[1] == 0) {
        if (spos > 0 && sub[-1] != '/')
          return false;
        spos++;
        sub++;
        return true;
      }
    }
  }
  return false;
}

namespace Mycila {
  MQTTClass MQTT;
} // namespace Mycila
