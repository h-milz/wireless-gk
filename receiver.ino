#include "Arduino.h"
#include "driver/i2s_tdm.h"
#include "WiFi.h"
#include "WebSocketsServer.h"

// WiFi Configuration
const char* ssid = "Your_SSID"; 
const char* password = "Your_PASSWORD"; 
const int serverPort = 81; 

// WebSockets server
WebSocketsServer webSocket(serverPort);

// I2S/TDM Configuration for PCM1681 DAC
i2s_chan_config_t i2s_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

i2s_chan_handle_t i2s_handle;

i2s_new_channel(&i2s_chan_cfg, &i2s_handle, NULL);

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
        .dout = GPIO_NUM_22,  // Data output
    },
};

void setup() {
    Serial.begin(115200);

    // Start the WiFi Access Point
    WiFi.softAP(ssid, password);

    // Initialize WebSockets server
    webSocket.begin();
    webSocket.onEvent([](WStype_t type, uint8_t* payload, size_t length) {
        if (type == WStype_BIN) {
            processIncomingData(payload, length); 
        }
    });

    // Initialize I2S/TDM and set the pin configuration
    i2s_channel_init_tdm_mode(i2s_handle, &i2s_tdm_cfg);

    // Enable the I2S channel
    i2s_channel_enable(i2s_handle);

    // Set up open-drain output for switches
    const int switch1Pin = 18; 
    const int switch2Pin = 19; 

    pinMode(switch1Pin, OUTPUT_OPEN_DRAIN);
    pinMode(switch2Pin, OUTPUT_OPEN_DRAIN);
}

void processIncomingData(uint8_t* payload, size_t length) {
    // Extract switch states from the LSBs of slot 7
    int switch1 = (payload[28] >> 1) & 1; // Switch 1
    int switch2 = payload[28] & 1;        // Switch 2

    // Output the switch states to GPIO in OPEN_DRAIN mode
    digitalWrite(switch1Pin, switch1);
    digitalWrite(switch2Pin, switch2);

    // Clear the two LSBs in slot 7 before sending to DAC
    payload[28] &= 0xFC;

    // Write the I2S/TDM data to the DAC
    size_t bytes_written;
    i2s_channel_write(i2s_handle, payload, length, &bytes_written, portMAX_DELAY);
}

void loop() {
    webSocket.loop(); // Handle WebSockets connections and data
}

