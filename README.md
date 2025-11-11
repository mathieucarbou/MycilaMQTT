# MycilaMQTT

[![Latest Release](https://img.shields.io/github/release/mathieucarbou/MycilaMQTT.svg)](https://GitHub.com/mathieucarbou/MycilaMQTT/releases/)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mathieucarbou/library/MycilaMQTT.svg)](https://registry.platformio.org/libraries/mathieucarbou/MycilaMQTT)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](code_of_conduct.md)

[![Build](https://github.com/mathieucarbou/MycilaMQTT/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaMQTT/actions/workflows/ci.yml)
[![GitHub latest commit](https://badgen.net/github/last-commit/mathieucarbou/MycilaMQTT)](https://GitHub.com/mathieucarbou/MycilaMQTT/commit/)
[![Gitpod Ready-to-Code](https://img.shields.io/badge/Gitpod-Ready--to--Code-blue?logo=gitpod)](https://gitpod.io/#https://github.com/mathieucarbou/MycilaMQTT)

A simple, efficient and modern MQTT/MQTTS client library for ESP32 Arduino projects, built on top of the Espressif ESP-IDF MQTT API.

## ✨ Features

- 🔄 **Automatic reconnection** with configurable intervals
- 📡 **Automatic resubscription** to topics after reconnection
- 🎯 **Simple configuration** - Easy integration with any configuration system
- 🔌 **Hot reload** - Quick restart with `end()` and `begin()`
- 💀 **Will topic management** - Automatic handling of last will and testament
- 🔐 **SSL/TLS support** - Both server certificates and CA certificate bundles
- ⚡ **Async/Sync modes** - Choose between blocking and non-blocking publish operations
- 📝 **Message retention** - Support for retained messages
- 🎨 **Wildcard subscriptions** - Full support for MQTT topic wildcards (`+` and `#`)
- 🧩 **Arduino 3 compatible** - Works with ESP-IDF 5
- 🪶 **Lightweight** - Minimal memory footprint with configurable buffer sizes
- 🎛️ **Highly configurable** - Fine-tune task priority, stack size, timeouts, and more

## 📋 Table of Contents

- [Installation](#-installation)
- [Quick Start](#-quick-start)
- [Configuration](#-configuration)
- [SSL/TLS Security](#-ssltls-security)
- [API Reference](#-api-reference)
- [Examples](#-examples)
- [Advanced Configuration](#-advanced-configuration)
- [Contributing](#-contributing)
- [License](#-license)

## 📦 Installation

### PlatformIO

Add the following to your `platformio.ini`:

```ini
lib_deps =
    mathieucarbou/MycilaMQTT@^6.1.2
```

### Arduino IDE

1. Go to **Sketch** > **Include Library** > **Manage Libraries**
2. Search for `MycilaMQTT`
3. Click **Install**

## 🚀 Quick Start

### Basic MQTT Connection

```cpp
#include <Arduino.h>
#include <MycilaMQTT.h>
#include <WiFi.h>

Mycila::MQTT mqtt;

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin("your-ssid", "your-password");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Configure MQTT
  Mycila::MQTT::Config config;
  config.server = "test.mosquitto.org";
  config.port = 1883;
  config.clientId = "my-esp32-client";
  config.willTopic = "my-device/status";

  // Set connection callback
  mqtt.onConnect([]() {
    Serial.println("MQTT connected!");
  });

  // Subscribe to topics
  mqtt.subscribe("my-device/commands", [](const std::string& topic, const std::string_view& payload) {
    Serial.printf("Received: %s = %.*s\n", topic.c_str(), payload.length(), payload.data());
  });

  // Start MQTT client
  mqtt.begin(config);
}

void loop() {
  // Publish messages
  if (mqtt.isConnected()) {
    mqtt.publish("my-device/sensor", "23.5");
  }
  delay(5000);
}
```

### With Authentication

```cpp
Mycila::MQTT::Config config;
config.server = "your-mqtt-broker.com";
config.port = 1883;
config.username = "your-username";
config.password = "your-password";
config.clientId = "my-esp32-client";
config.willTopic = "my-device/status";

mqtt.begin(config);
```

## ⚙️ Configuration

### Basic Configuration Options

```cpp
Mycila::MQTT::Config config;

// Required
config.server = "mqtt.example.com";    // MQTT broker hostname or IP
config.port = 1883;                     // MQTT broker port (1883 for plain, 8883 for SSL)
config.clientId = "unique-client-id";   // Unique client identifier
config.willTopic = "device/status";     // Last will topic

// Optional
config.username = "user";               // MQTT username (if required)
config.password = "pass";               // MQTT password (if required)
config.secured = false;                 // Enable SSL/TLS (default: false)
config.keepAlive = 60;                  // Keep-alive interval in seconds (default: 60)
```

### SSL/TLS Configuration

#### Option 1: Server Certificate (for self-signed certificates)

```cpp
config.secured = true;
config.port = 8885;
config.serverCertPtr = certificate_pem;  // Pointer to PEM certificate string

// Or load from string
config.serverCert = loadCertificateFromFile();
```

#### Option 2: CA Certificate Bundle (for official certificates)

```cpp
extern const uint8_t ca_certs_bundle_start[] asm("_binary__pio_data_cacerts_bin_start");
extern const uint8_t ca_certs_bundle_end[] asm("_binary__pio_data_cacerts_bin_end");

config.secured = true;
config.port = 8886;
config.certBundle = ca_certs_bundle_start;
config.certBundleSize = ca_certs_bundle_end - ca_certs_bundle_start;
```

## 🔐 SSL/TLS Security

MycilaMQTT supports two SSL/TLS authentication methods:

### 1. Server Certificate Authentication

Use this when connecting to MQTT brokers with self-signed certificates (common in internal/home networks).

**Example:** [examples/server_cert/server_cert.ino](examples/server_cert/server_cert.ino)

```cpp
static const char* server_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIEAzCCAuugAwIBAgIUBY1hlCGvdj4NhBXkZ/uLUZNILAwwDQYJKoZIhvcNAQEL
...
-----END CERTIFICATE-----
)EOF";

config.secured = true;
config.serverCertPtr = server_cert;
```

### 2. CA Certificate Bundle

Use this when connecting to MQTT brokers with officially signed certificates (e.g., AWS IoT, Azure IoT Hub, public MQTT brokers).

The certificate bundle contains trusted CA certificates from major certificate authorities, allowing your ESP32 to validate any properly signed certificate.

**Example:** [examples/cacerts/cacerts.ino](examples/cacerts/cacerts.ino)

#### Generate Certificate Bundle

1. Add to your `platformio.ini`:

```ini
[env:myenv]
platform = espressif32
framework = arduino
extra_scripts = pre:tools/cacerts.py
custom_cacert_url = https://curl.se/ca/cacert.pem
board_build.embed_files = .pio/data/cacerts.bin
```

2. The `cacerts.py` script will automatically:
   - Download the CA certificate bundle from Mozilla
   - Convert it to ESP32 format
   - Embed it in your firmware

3. Reference it in your code:

```cpp
extern const uint8_t ca_certs_bundle_start[] asm("_binary__pio_data_cacerts_bin_start");
extern const uint8_t ca_certs_bundle_end[] asm("_binary__pio_data_cacerts_bin_end");

config.certBundle = ca_certs_bundle_start;
config.certBundleSize = ca_certs_bundle_end - ca_certs_bundle_start;
```

## 📚 API Reference

### Initialization

```cpp
void begin(Config config)        // Start MQTT client with configuration
void end()                        // Stop MQTT client and disconnect
```

### Publishing

```cpp
bool publish(const char* topic, const std::string_view& payload, bool retain = false)
```

- Returns `true` if message was successfully queued/sent
- In async mode, messages are queued; in sync mode, waits for acknowledgment

### Subscribing

```cpp
void subscribe(std::string topic, MessageCallback callback)
void unsubscribe(const char* topic)
```

**Wildcard Support:**
- `+` - Single-level wildcard (e.g., `home/+/temperature`)
- `#` - Multi-level wildcard (e.g., `home/#`)

**Example:**

```cpp
mqtt.subscribe("home/+/temperature", [](const std::string& topic, const std::string_view& payload) {
  Serial.printf("Topic: %s, Value: %.*s\n", topic.c_str(), payload.length(), payload.data());
});

mqtt.subscribe("sensors/#", [](const std::string& topic, const std::string_view& payload) {
  // Matches sensors/room1/temp, sensors/room2/humidity, etc.
});
```

### Callbacks

```cpp
void onConnect(ConnectedCallback callback)  // Called when MQTT connects
```

### Status

```cpp
bool isEnabled()                 // Returns true if MQTT client is initialized
bool isConnected()               // Returns true if connected to broker
const char* getLastError()       // Returns last error message (or nullptr)
```

### Async Mode

```cpp
void setAsync(bool async)        // Enable/disable async publishing mode
bool isAsync()                   // Check if async mode is enabled
```

- **Sync mode (default):** `publish()` blocks until message is sent
- **Async mode:** `publish()` queues message and returns immediately

### Advanced Configuration Hook

```cpp
void setConfigHook(std::function<void(esp_mqtt_client_config_t& cfg)> hook)
```

Allows direct modification of the ESP-IDF MQTT configuration before client initialization.

## 📖 Examples

### Basic MQTT

[examples/mqtt/mqtt.ino](examples/mqtt/mqtt.ino) - Simple MQTT connection with publish/subscribe

### SSL with Server Certificate

[examples/server_cert/server_cert.ino](examples/server_cert/server_cert.ino) - MQTTS with server certificate validation

### SSL with CA Bundle

[examples/cacerts/cacerts.ino](examples/cacerts/cacerts.ino) - MQTTS with CA certificate bundle

## 🔧 Advanced Configuration

MycilaMQTT exposes several build-time configuration macros that can be defined in your `platformio.ini` or before including the library:

```ini
build_flags =
    -D MYCILA_MQTT_BUFFER_SIZE=2048        # Buffer size for incoming/outgoing messages (default: 1024)
    -D MYCILA_MQTT_STACK_SIZE=6144         # MQTT task stack size (default: 4096)
    -D MYCILA_MQTT_TASK_PRIORITY=5         # MQTT task priority (default: 5)
    -D MYCILA_MQTT_KEEPALIVE=120           # Keep-alive interval in seconds (default: 60)
    -D MYCILA_MQTT_RECONNECT_INTERVAL=5    # Reconnect interval in seconds (default: 10)
    -D MYCILA_MQTT_NETWORK_TIMEOUT=15      # Network timeout in seconds (default: 10)
    -D MYCILA_MQTT_CLEAN_SESSION=true      # Clean session flag (default: false)
    -D MYCILA_MQTT_OUTBOX_SIZE=4096        # Outbox size for QoS > 0 messages (default: 0)
    -D MYCILA_MQTT_DEBUG                    # Enable debug logging
```

### Configuration Parameters Explained

| Parameter | Description | Default |
|-----------|-------------|---------|
| `MYCILA_MQTT_BUFFER_SIZE` | Size of the buffer for incoming/outgoing messages. Increase if handling large payloads | 1024 |
| `MYCILA_MQTT_STACK_SIZE` | Stack size for the MQTT task. Increase if experiencing stack overflows | 4096 |
| `MYCILA_MQTT_TASK_PRIORITY` | FreeRTOS task priority for MQTT operations | 5 |
| `MYCILA_MQTT_KEEPALIVE` | Keep-alive interval sent to broker (seconds) | 60 |
| `MYCILA_MQTT_RECONNECT_INTERVAL` | Time between reconnection attempts (seconds) | 10 |
| `MYCILA_MQTT_NETWORK_TIMEOUT` | Network operation timeout (seconds) | 10 |
| `MYCILA_MQTT_CLEAN_SESSION` | Whether to start with a clean session | false |
| `MYCILA_MQTT_OUTBOX_SIZE` | Outbox size for storing QoS > 0 messages | 0 |
| `MYCILA_MQTT_RETRANSMIT_TIMEOUT` | Retransmit timeout for QoS > 0 messages (seconds) | 0 |

### Logger Support

MycilaMQTT can integrate with MycilaLogger for enhanced logging:

```ini
build_flags =
    -D MYCILA_LOGGER_SUPPORT
```

## 🎯 Best Practices

1. **Buffer Size:** Ensure `MYCILA_MQTT_BUFFER_SIZE` is large enough for your largest message. The library does not support message fragmentation/reassembly.

2. **Client ID:** Use unique client IDs to prevent connection conflicts. Consider using MAC address or chip ID:
   ```cpp
   config.clientId = "esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
   ```

3. **Will Topic:** Always configure a will topic for device status monitoring:
   ```cpp
   config.willTopic = "devices/" + clientId + "/status";
   ```
   The library automatically publishes "online" on connect and "offline" on disconnect.

4. **Async Mode:** Use async mode for high-frequency publishing to avoid blocking:
   ```cpp
   mqtt.setAsync(true);
   ```

5. **SSL/TLS:** For production deployments, always use SSL/TLS with proper certificate validation.

## 🐛 Troubleshooting

### Connection Issues

- Verify broker hostname/IP and port
- Check WiFi connection is stable
- Ensure firewall allows MQTT traffic
- Verify credentials if authentication is enabled

### SSL/TLS Issues

- Ensure certificate format is correct (PEM)
- Verify certificate hasn't expired
- Check that the certificate matches the server hostname
- Try increasing `MYCILA_MQTT_NETWORK_TIMEOUT`

### Memory Issues

- Increase `MYCILA_MQTT_STACK_SIZE` if experiencing crashes
- Reduce `MYCILA_MQTT_BUFFER_SIZE` if running low on heap
- Monitor heap with `ESP.getFreeHeap()`

### Message Delivery Issues

- Check `MYCILA_MQTT_BUFFER_SIZE` is sufficient for your payload
- Verify topic names don't contain invalid characters
- Ensure you're connected before publishing: `mqtt.isConnected()`

## 🤝 Contributing

Contributions are welcome! Please read the [Code of Conduct](CODE_OF_CONDUCT.md) before contributing.

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 👤 Author

**Mathieu Carbou**
- GitHub: [@mathieucarbou](https://github.com/mathieucarbou)

## ⭐ Star History

If you find this library useful, please consider giving it a star on GitHub!
