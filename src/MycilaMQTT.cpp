// SPDX-License-Identifier: MIT
/*
 * Copyright (C) Mathieu Carbou
 */
#include <MycilaMQTT.h>

#include <esp32-hal-log.h>
#include <esp_crt_bundle.h>

#include <algorithm>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#define TAG "MQTT"

void Mycila::MQTT::begin(MQTT::Config config) {
  if (_state != MQTT::State::MQTT_DISABLED)
    return;

  _config = std::move(config);

  if (_config.server.empty() || _config.port <= 0) {
    ESP_LOGE(TAG, "MQTT disabled: Invalid server, port or base topic");
    return;
  }

  ESP_LOGI(TAG, "Enable MQTT...");

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
    ESP_LOGE(TAG, "Failed to create MQTT client");
    return;
  }
  ESP_ERROR_CHECK(esp_mqtt_client_register_event(_mqttClient, MQTT_EVENT_ANY, _mqttEventHandler, this));
  _state = MQTT::State::MQTT_CONNECTING;
  ESP_ERROR_CHECK(esp_mqtt_client_start(_mqttClient));
}

void Mycila::MQTT::end() {
  if (_state == MQTT::State::MQTT_DISABLED)
    return;

  ESP_LOGI(TAG, "Disable MQTT...");
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
  std::string t;
  {
    // unique_lock: insertion mutates the list structure (links, size)
    std::unique_lock<std::shared_mutex> lock(_listenersMutex);
    // the listener struct is not movable (atomic member), so it is built in place;
    // the topic and callback are moved into the node, no copy, no extra allocation
    _listeners.emplace_back();
    MQTTMessageListener& node = _listeners.back();
    node.topic = std::move(topic);
    node.callback = std::move(callback);
    // copy the topic for use after the lock is released
    t = node.topic;
  }
  if (isConnected()) {
    // esp-mqtt calls are never issued while `_listenersMutex` is held: esp-mqtt holds its own
    // internal lock across event dispatch, so taking ours first would deadlock the two tasks.
    if (esp_mqtt_client_subscribe(_mqttClient, t.c_str(), 0) != -1) {
      ESP_LOGD(TAG, "Subscribed to: %s", t.c_str());
    } else {
      ESP_LOGE(TAG, "Failed to subscribe to: %s", t.c_str());
    }
  } else {
    ESP_LOGD(TAG, "Will subscribe (when connected) to: %s", t.c_str());
  }
}

void Mycila::MQTT::unsubscribe(const char* topic) {
  ESP_LOGD(TAG, "Unsubscribing from: %s...", topic);
  {
    // shared_lock: flipping `active` only touches element data, not list structure
    std::shared_lock<std::shared_mutex> lock(_listenersMutex);
    for (auto& listener : _listeners) {
      if (listener.topic == topic) {
        // tombstone: the node is physically removed by the MQTT task, at the next event
        listener.active.store(false, std::memory_order_relaxed);
      }
    }
  }
  // lock released: esp-mqtt calls are never issued while `_listenersMutex` is held.
  // Safe to call from within a listener callback (the MQTT task re-enters esp-mqtt recursively).
  if (isConnected())
    esp_mqtt_client_unsubscribe(_mqttClient, topic);
}

std::vector<std::string> Mycila::MQTT::getSubscribedTopics() const {
  std::shared_lock<std::shared_mutex> lock(_listenersMutex);
  std::vector<std::string> topics;
  topics.reserve(_listeners.size());
  for (const auto& listener : _listeners) {
    if (listener.active)
      topics.push_back(listener.topic);
  }
  return topics;
}

void Mycila::MQTT::_mqttEventHandler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
  esp_mqtt_client_handle_t mqttClient = event->client;
  Mycila::MQTT* mqtt = (Mycila::MQTT*)event_handler_arg;

  // lock the list and cleanup inactive listeners
  mqtt->_cleanupInactiveListeners();

  // if state is MQTT_DISABLED, we should ignore events as the client is no longer active
  MQTT::State state = mqtt->_state;
  if (state == MQTT::State::MQTT_DISABLED)
    return;

  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_ERROR:
      switch (event->error_handle->error_type) {
        case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
#ifdef MYCILA_MQTT_DEBUG
          ESP_LOGD(TAG, "MQTT_EVENT_ERROR: Connection refused");
#endif
          mqtt->_lastError = "Connection refused";
          break;
        case MQTT_ERROR_TYPE_TCP_TRANSPORT:
#ifdef MYCILA_MQTT_DEBUG
          ESP_LOGD(TAG, "MQTT_EVENT_ERROR: TCP transport error: %s", strerror(event->error_handle->esp_transport_sock_errno));
#endif
          mqtt->_lastError = "TCP transport error";
          break;
        default:
#ifdef MYCILA_MQTT_DEBUG
          ESP_LOGD(TAG, "MQTT_EVENT_ERROR: Unknown error");
#endif
          mqtt->_lastError = "Unknown error";
          break;
      }
      break;
    case MQTT_EVENT_CONNECTED:
#ifdef MYCILA_MQTT_DEBUG
      ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
#endif
      if (!mqtt->_state.compare_exchange_strong(state, MQTT::State::MQTT_CONNECTED)) {
        // state has changed => user ended ?
        return;
      }
      mqtt->publish(mqtt->_config.willTopic.c_str(), "online", true);
      // Snapshot the active listeners under a shared lock, then re-subscribe outside the lock:
      // esp-mqtt calls are never issued while `_listenersMutex` is held.
      // Node pointers are stable here: removals only happen on the MQTT task (deferred to the
      // next handler entry), and additions never invalidate existing nodes.
      {
        std::vector<MQTTMessageListener*> listeners;
        {
          std::shared_lock<std::shared_mutex> lock(mqtt->_listenersMutex);
          listeners.reserve(mqtt->_listeners.size());
          for (auto& _listener : mqtt->_listeners)
            if (_listener.active.load(std::memory_order_relaxed))
              listeners.push_back(&_listener);
        }
        for (MQTTMessageListener* _listener : listeners) {
          // re-check in case unsubscribe() was called by a user thread meanwhile
          if (!_listener->active.load(std::memory_order_relaxed))
            continue;
          esp_mqtt_client_subscribe(mqttClient, _listener->topic.c_str(), 0);
#ifdef MYCILA_MQTT_DEBUG
          ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED: %s", _listener->topic.c_str());
#endif
        }
      }
      if (mqtt->_onConnect)
        mqtt->_onConnect();
      break;
    case MQTT_EVENT_DISCONNECTED:
#ifdef MYCILA_MQTT_DEBUG
      ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
#endif
      if (!mqtt->_state.compare_exchange_strong(state, MQTT::State::MQTT_DISCONNECTED)) {
        // state has changed => user ended ?
        return;
      }
      break;
    case MQTT_EVENT_SUBSCRIBED:
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      break;
    case MQTT_EVENT_DATA: {
      std::string topic(event->topic, event->topic_len);
      std::string_view data(event->data, event->data_len);
#ifdef MYCILA_MQTT_DEBUG
      ESP_LOGD(TAG, "MQTT_EVENT_DATA: %s len=%" PRIu32, topic.c_str(), static_cast<uint32_t>(data.length()));
#endif
      // Snapshot the matching listeners under a shared lock, then invoke the callbacks
      // outside the lock: callbacks may call subscribe()/unsubscribe() without deadlocking.
      std::vector<MQTTMessageListener*> listeners;
      {
        std::shared_lock<std::shared_mutex> lock(mqtt->_listenersMutex);
        listeners.reserve(mqtt->_listeners.size());
        for (auto& listener : mqtt->_listeners)
          if (listener.active.load(std::memory_order_relaxed) && _topicMatches(listener.topic.c_str(), topic.c_str()))
            listeners.push_back(&listener);
      }
      // `std::list` nodes are stable: pointers remain valid because removals only happen on the
      // MQTT task (deferred to the next handler entry), and additions never invalidate nodes.
      for (MQTTMessageListener* listener : listeners) {
        // re-check in case unsubscribe() was called by an earlier callback of this same message
        if (listener->active.load(std::memory_order_relaxed))
          listener->callback(topic, data);
      }
      break;
    }
    case MQTT_EVENT_BEFORE_CONNECT:
#ifdef MYCILA_MQTT_DEBUG
      ESP_LOGD(TAG, "MQTT_EVENT_BEFORE_CONNECT");
#endif
      if (!mqtt->_state.compare_exchange_strong(state, MQTT::State::MQTT_CONNECTING)) {
        // state has changed => user ended ?
        return;
      }
      break;
    case MQTT_EVENT_DELETED:
// see OUTBOX_EXPIRED_TIMEOUT_MS and MQTT_REPORT_DELETED_MESSAGES
#ifdef MYCILA_MQTT_DEBUG
      ESP_LOGD(TAG, "MQTT_EVENT_DELETED: %d", event->msg_id);
#endif
      break;
    default:
      break;
  }
}

bool Mycila::MQTT::_topicMatches(const char* sub, const char* topic) {
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

void Mycila::MQTT::_cleanupInactiveListeners() {
  std::unique_lock<std::shared_mutex> lock(_listenersMutex);
  _listeners.remove_if([](const MQTTMessageListener& listener) {
    return !listener.active;
  });
}
