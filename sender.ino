#include "Arduino.h"
#include "driver/i2s_tdm.h"
#include "WiFi.h"
#include "WebSocketsClient.h"

// WiFi Configuration
const char* ssid = "Your_SSID";
const char* password = "Your_PASSWORD";
const char* serverIP = "192.168.4.1";
const int serverPort = 81;

// GPIO for ADC reset
const int adcResetPin = 33;

// WebSockets client
WebSocketsClient webSocket;

// I2S/TDM Configuration for CS5368 ADC
i2s_chan_config_t i2s_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

i2s_chan_handle_t i2s_handle;

i2s_new_channel(&i2s_chan_cfg, &i2s_handle, NULL);

// I2S/TDM Configuration with 24-bit audio and MCLK output
i2s_tdm_config_t i2s_tdm_cfg = {
    .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(32000), // 32 kHz sample rate
    .slot_cfg = I2S_TDM_PCM_SHORT_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_24BIT, I2S_SLOT_MODE_MONO,
        I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3 |
        I2S_TDM_SLOT4 | I2S_TDM_SLOT5 | I2S_TDM_SLOT6 | I2S_TDM_SLOT7),
    .gpio_cfg = {
        .mclk = GPIO_NUM_0,   // Master Clock output
        .bclk = GPIO_NUM_26,  // Bit Clock
        .ws = GPIO_NUM_25,    // Frame Sync
        .din = GPIO_NUM_22,   // Data input
    },
};

void setup() {
    Serial.begin(115200);

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
    }

    // Initialize WebSockets client
    webSocket.begin(serverIP, serverPort, "/");
    webSocket.onEvent([](WStype_t type, uint8_t* payload, size_t length) {
        if (type == WStype_CONNECTED) {
            Serial.println("WebSockets Connected");
        }
    });

    // ADC reset setup
    pinMode(adcResetPin, OUTPUT);
    digitalWrite(adcResetPin, LOW); // Keep reset low

    // Initialize I2S/TDM and set the pin configuration
    i2s_channel_init_tdm_mode(i2s_handle, &i2s_tdm_cfg);

    // Enable the I2S channel and set ADC reset high after initialization
    i2s_channel_enable(i2s_handle);
    digitalWrite(adcResetPin, HIGH); // Release ADC reset
}

void loop() {
    uint8_t buffer[256]; // Buffer to hold I2S data
    size_t bytes_read;
    bool hasData = false;

    // Poll the I2S channel and wait for data
    do {
        i2s_channel_read(i2s_handle, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
        hasData = (bytes_read > 0);

        if (!hasData) {
            delayMicroseconds(10); // Busy wait if no data is available
        }
    } while (!hasData);

    // Get switch states
    int switch1 = digitalRead(2); // Example GPIO for Switch 1
    int switch2 = digitalRead(4); // Example GPIO for Switch 2

    // Insert the switch states into the LSBs of the potentiometer data (slot 7)
    buffer[28] = (buffer[28] & 0xFC) | ((switch1 << 1) | switch2); // slot 7 is at byte 28 (given 32 kHz)

    // Send the I2S/TDM data with embedded switch bits
    webSocket.sendBIN(buffer, bytes_read);

    delay(1); // Small delay for pacing
}
#include "Arduino.h"
#include "driver/i2s_tdm.h"
#include "WiFi.h"
#include "WebSocketsClient.h"

// WiFi Configuration
const char* ssid = "Your_SSID";
const char* password = "Your_PASSWORD";
const char* serverIP = "192.168.4.1";
const int serverPort = 81;

// GPIO for ADC reset
const int adcResetPin = 33;

// WebSockets client
WebSocketsClient webSocket;

// I2S/TDM Configuration for CS5368 ADC
i2s_chan_config_t i2s_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

i2s_chan_handle_t i2s_handle;

i2s_new_channel(&i2s_chan_cfg, &i2s_handle, NULL);

// I2S/TDM Configuration with 24-bit audio and MCLK output
i2s_tdm_config_t i2s_tdm_cfg = {
    .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(32000), // 32 kHz sample rate
    .slot_cfg = I2S_TDM_PCM_SHORT_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_24BIT, I2S_SLOT_MODE_MONO,
        I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3 |
        I2S_TDM_SLOT4 | I2S_TDM_SLOT5 | I2S_TDM_SLOT6 | I2S_TDM_SLOT7),
    .gpio_cfg = {
        .mclk = GPIO_NUM_0,   // Master Clock output
        .bclk = GPIO_NUM_26,  // Bit Clock
        .ws = GPIO_NUM_25,    // Frame Sync
        .din = GPIO_NUM_22,   // Data input
    },
};

void setup() {
    Serial.begin(115200);

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
    }

    // Initialize WebSockets client
    webSocket.begin(serverIP, serverPort, "/");
    webSocket.onEvent([](WStype_t type, uint8_t* payload, size_t length) {
        if (type == WStype_CONNECTED) {
            Serial.println("WebSockets Connected");
        }
    });

    // ADC reset setup
    pinMode(adcResetPin, OUTPUT);
    digitalWrite(adcResetPin, LOW); // Keep reset low

    // Initialize I2S/TDM and set the pin configuration
    i2s_channel_init_tdm_mode(i2s_handle, &i2s_tdm_cfg);

    // Enable the I2S channel and set ADC reset high after initialization
    i2s_channel_enable(i2s_handle);
    digitalWrite(adcResetPin, HIGH); // Release ADC reset
}

void loop() {
    uint8_t buffer[256]; // Buffer to hold I2S data
    size_t bytes_read;
    bool hasData = false;

    // Poll the I2S channel and wait for data
    do {
        i2s_channel_read(i2s_handle, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
        hasData = (bytes_read > 0);

        if (!hasData) {
            delayMicroseconds(10); // Busy wait if no data is available
        }
    } while (!hasData);

    // Get switch states
    int switch1 = digitalRead(2); // Example GPIO for Switch 1
    int switch2 = digitalRead(4); // Example GPIO for Switch 2

    // Insert the switch states into the LSBs of the potentiometer data (slot 7)
    buffer[28] = (buffer[28] & 0xFC) | ((switch1 << 1) | switch2); // slot 7 is at byte 28 (given 32 kHz)

    // Send the I2S/TDM data with embedded switch bits
    webSocket.sendBIN(buffer, bytes_read);

    delay(1); // Small delay for pacing
}

