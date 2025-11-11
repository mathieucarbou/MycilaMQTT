// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#include <MycilaMQTT.h>

#include <esp32-hal-log.h>
#include <esp_crt_bundle.h>

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#ifdef MYCILA_LOGGER_SUPPORT
  #include <MycilaLogger.h>
extern Mycila::Logger logger;
  #define LOGD(tag, format, ...) logger.debug(tag, format, ##__VA_ARGS__)
  #define LOGI(tag, format, ...) logger.info(tag, format, ##__VA_ARGS__)
  #define LOGW(tag, format, ...) logger.warn(tag, format, ##__VA_ARGS__)
  #define LOGE(tag, format, ...) logger.error(tag, format, ##__VA_ARGS__)
#else
  #define LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
  #define LOGI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
  #define LOGW(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
  #define LOGE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#endif

#define TAG "MQTT"

void Mycila::MQTT::begin(MQTT::Config config) {
  if (_state != MQTT::State::MQTT_DISABLED)
    return;

  _config = std::move(config);

  if (_config.server.empty() || _config.port <= 0) {
    LOGE(TAG, "MQTT disabled: Invalid server, port or base topic");
    return;
  }

  LOGI(TAG, "Enable MQTT...");

  const bool auth = !_config.username.empty() && !_config.password.empty();

  if (_config.certBundle) {
    esp_crt_bundle_set(_config.certBundle, _config.certBundleSize);
  }
  esp_mqtt_client_config_t* cfg = new esp_mqtt_client_config_t();

  cfg->broker.address.uri = nullptr;
  cfg->broker.address.hostname = _config.server.c_str();
  cfg->broker.address.transport = _config.secured ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;
  cfg->broker.address.path = nullptr;
  cfg->broker.address.port = _config.port;

  cfg->broker.verification.use_global_ca_store = false;
  cfg->broker.verification.crt_bundle_attach = _config.secured && _config.certBundle ? esp_crt_bundle_attach : nullptr;
  cfg->broker.verification.certificate = !_config.secured ? nullptr : (!_config.serverCert.empty() ? _config.serverCert.c_str() : _config.serverCertPtr),
  cfg->broker.verification.certificate_len = 0;
  cfg->broker.verification.psk_hint_key = nullptr;
  cfg->broker.verification.skip_cert_common_name_check = true;
  cfg->broker.verification.alpn_protos = nullptr;
  cfg->broker.verification.common_name = nullptr;

  cfg->credentials.username = auth ? _config.username.c_str() : nullptr;
  cfg->credentials.client_id = _config.clientId.c_str();
  cfg->credentials.set_null_client_id = false;
  cfg->credentials.authentication.password = auth ? _config.password.c_str() : nullptr;
  cfg->credentials.authentication.certificate = nullptr;
  cfg->credentials.authentication.certificate_len = 0;
  cfg->credentials.authentication.key = nullptr;
  cfg->credentials.authentication.key_len = 0;
  cfg->credentials.authentication.key_password = nullptr;
  cfg->credentials.authentication.key_password_len = 0;
  cfg->credentials.authentication.use_secure_element = false;
  cfg->credentials.authentication.ds_data = nullptr;
  cfg->credentials.authentication.use_ecdsa_peripheral = false;
  cfg->credentials.authentication.ecdsa_key_efuse_blk = 0;

  cfg->session.last_will.topic = _config.willTopic.c_str();
  cfg->session.last_will.msg = "offline";
  cfg->session.last_will.msg_len = 7;
  cfg->session.last_will.qos = 0;
  cfg->session.last_will.retain = true;
  cfg->session.disable_clean_session = !MYCILA_MQTT_CLEAN_SESSION;
  cfg->session.keepalive = _config.keepAlive;
  cfg->session.disable_keepalive = false;
  cfg->session.protocol_ver = esp_mqtt_protocol_ver_t::MQTT_PROTOCOL_UNDEFINED;
  cfg->session.message_retransmit_timeout = MYCILA_MQTT_RETRANSMIT_TIMEOUT * 1000;

  cfg->network.reconnect_timeout_ms = MYCILA_MQTT_RECONNECT_INTERVAL * 1000;
  cfg->network.timeout_ms = MYCILA_MQTT_NETWORK_TIMEOUT * 1000;
  cfg->network.refresh_connection_after_ms = 0;
  cfg->network.disable_auto_reconnect = false;
  cfg->network.tcp_keep_alive_cfg.keep_alive_enable = true;
  cfg->network.tcp_keep_alive_cfg.keep_alive_idle = 60;
  cfg->network.tcp_keep_alive_cfg.keep_alive_interval = 20;
  cfg->network.tcp_keep_alive_cfg.keep_alive_count = 3;
  cfg->network.transport = nullptr;
  cfg->network.if_name = nullptr;

  cfg->task.priority = MYCILA_MQTT_TASK_PRIORITY;
  cfg->task.stack_size = MYCILA_MQTT_STACK_SIZE;

  cfg->buffer.size = MYCILA_MQTT_BUFFER_SIZE;
  cfg->buffer.out_size = MYCILA_MQTT_BUFFER_SIZE;

  cfg->outbox.limit = MYCILA_MQTT_OUTBOX_SIZE;

  if (_configHook)
    _configHook(*cfg);

  _lastError = nullptr;
  _mqttClient = esp_mqtt_client_init(cfg);
  delete cfg;

  if (!_mqttClient) {
    LOGE(TAG, "Failed to create MQTT client");
    return;
  }
  ESP_ERROR_CHECK(esp_mqtt_client_register_event(_mqttClient, MQTT_EVENT_ANY, _mqttEventHandler, this));
  ESP_ERROR_CHECK(esp_mqtt_client_start(_mqttClient));
  _state = MQTT::State::MQTT_CONNECTING;
}

void Mycila::MQTT::end() {
  if (_state == MQTT::State::MQTT_DISABLED)
    return;

  LOGI(TAG, "Disable MQTT...");
  esp_mqtt_client_publish(_mqttClient, _config.willTopic.c_str(), "offline", 7, 0, true);
  _state = MQTT::State::MQTT_DISABLED;
  esp_mqtt_client_disconnect(_mqttClient);
  esp_mqtt_client_stop(_mqttClient);
  esp_mqtt_client_destroy(_mqttClient);
  _mqttClient = nullptr;
}

bool Mycila::MQTT::publish(const char* topic, const std::string_view& payload, bool retain) {
  if (!isConnected())
    return false;
  if (_async)
    return esp_mqtt_client_enqueue(_mqttClient, topic, payload.begin(), payload.length(), 0, retain, true) >= 0;
  else
    return esp_mqtt_client_publish(_mqttClient, topic, payload.begin(), payload.length(), 0, retain) >= 0;
}

void Mycila::MQTT::subscribe(std::string topic, MQTT::MessageCallback callback) {
  _listeners.push_back({std::move(topic), std::move(callback)});
  if (isConnected()) {
    const char* t = _listeners.back().topic.c_str();
    if (esp_mqtt_client_subscribe(_mqttClient, t, 0) != -1) {
      LOGD(TAG, "Subscribed to: %s", t);
    } else {
      LOGE(TAG, "Failed to subscribe to: %s", t);
    }
  } else {
    LOGD(TAG, "Will subscribe (when connected) to: %s", _listeners.back().topic.c_str());
  }
}

void Mycila::MQTT::unsubscribe(const char* topic) {
  LOGD(TAG, "Unsubscribing from: %s...", topic);
  esp_mqtt_client_unsubscribe(_mqttClient, topic);
  _listeners.remove_if([topic](const MQTTMessageListener& listener) {
    return listener.topic == topic;
  });
}

void Mycila::MQTT::_mqttEventHandler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
  esp_mqtt_client_handle_t mqttClient = event->client;
  Mycila::MQTT* mqtt = (Mycila::MQTT*)event_handler_arg;
  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_ERROR:
      switch (event->error_handle->error_type) {
        case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
#ifdef MYCILA_MQTT_DEBUG
          LOGD(TAG, "MQTT_EVENT_ERROR: Connection refused");
#endif
          mqtt->_lastError = "Connection refused";
          break;
        case MQTT_ERROR_TYPE_TCP_TRANSPORT:
#ifdef MYCILA_MQTT_DEBUG
          LOGD(TAG, "MQTT_EVENT_ERROR: TCP transport error: %s", strerror(event->error_handle->esp_transport_sock_errno));
#endif
          mqtt->_lastError = "TCP transport error";
          break;
        default:
#ifdef MYCILA_MQTT_DEBUG
          LOGD(TAG, "MQTT_EVENT_ERROR: Unknown error");
#endif
          mqtt->_lastError = "Unknown error";
          break;
      }
      break;
    case MQTT_EVENT_CONNECTED:
      mqtt->_state = MQTT::State::MQTT_CONNECTED;
      mqtt->publish(mqtt->_config.willTopic.c_str(), "online", true);
#ifdef MYCILA_MQTT_DEBUG
      LOGD(TAG, "MQTT_EVENT_CONNECTED: Subscribing to %u topics...", mqtt->_listeners.size());
#endif
      for (auto& _listener : mqtt->_listeners) {
        esp_mqtt_client_subscribe(mqttClient, _listener.topic.c_str(), 0);
#ifdef MYCILA_MQTT_DEBUG
        LOGD(TAG, "MQTT_EVENT_CONNECTED: %s", _listener.topic.c_str());
#endif
      }
      if (mqtt->_onConnect)
        mqtt->_onConnect();
      break;
    case MQTT_EVENT_DISCONNECTED:
#ifdef MYCILA_MQTT_DEBUG
      LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
#endif
      mqtt->_state = MQTT::State::MQTT_DISCONNECTED;
      break;
    case MQTT_EVENT_SUBSCRIBED:
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      break;
    case MQTT_EVENT_DATA: {
      std::string topic(event->topic, event->topic_len);
      std::string_view data(event->data, event->data_len);
#ifdef MYCILA_MQTT_DEBUG
      LOGD(TAG, "MQTT_EVENT_DATA: %s len=%" PRIu32, topic.c_str(), static_cast<uint32_t>(data.length()));
#endif
      for (auto& listener : mqtt->_listeners)
        if (_topicMatches(listener.topic.c_str(), topic.c_str()))
          listener.callback(topic, data);
      break;
    }
    case MQTT_EVENT_BEFORE_CONNECT:
#ifdef MYCILA_MQTT_DEBUG
      LOGD(TAG, "MQTT_EVENT_BEFORE_CONNECT");
#endif
      mqtt->_state = MQTT::State::MQTT_CONNECTING;
      break;
    case MQTT_EVENT_DELETED:
// see OUTBOX_EXPIRED_TIMEOUT_MS and MQTT_REPORT_DELETED_MESSAGES
#ifdef MYCILA_MQTT_DEBUG
      LOGD(TAG, "MQTT_EVENT_DELETED: %d", event->msg_id);
#endif
      break;
    default:
      break;
  }
}

bool Mycila::MQTT::_topicMatches(const char* sub, const char* topic) {
  // LOGD(TAG, "Match: %s vs %s ?", sub, topic);
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
