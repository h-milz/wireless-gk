/* 
    Copyright (C) 2024 Harald Milz <hm@seneca.muc.de> 

    This file is part of Wireless-GK.
    
    Wireless-GK is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Wireless-GK is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Wireless-GK.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _WIRELESS_GK_H
#define _WIRELESS_GK_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h> 
#include <dirent.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "lwip/err.h"
#include <lwip/netdb.h>
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/errno.h"

// #define TX_DEBUG
// #define RX_DEBUG

// TODO remove for production
#pragma GCC diagnostic ignored "-Wunused-variable"

// during development, we use STD with PCM1808 ADC and PCM5102 DAC
#define I2S_STD
// #define I2S_TDM

#ifdef I2S_STD
#include "driver/i2s_std.h"
#elif I2S_TDM
#include "driver/i2s_tdm.h"
#endif

#define I2S_MCLK_MULTIPLE I2S_MCLK_MULTIPLE_256
#define I2S_DATA_BIT_WIDTH I2S_DATA_BIT_WIDTH_32BIT

#define I2S_NUM                 I2S_NUM_AUTO
#define NSAMPLES                60                      // the number of samples we want to send in a batch
#define NUM_SLOTS               8                       // TDM256, 8 slots per sample
#define SLOT_WIDTH              32                      // 
#define DMA_BUFFER_COUNT        4                       // Number of DMA buffers. 
                                                        // 4 is enough because we pick up each individual one
#define DMA_BUFFER_SIZE         NSAMPLES * NUM_SLOTS * SLOT_WIDTH / 8  // Size of each DMA buffer

#define SAMPLE_RATE             44100                   // 44100

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#if (defined RX_DEBUG || defined TX_DEBUG)
#define NUM 20
typedef struct { 
    uint8_t loc;        // location
    uint32_t time;      // timestamp in µs
    uint8_t *ptr;       // pointer to buffer
    uint32_t size;        // data size
    uint32_t uint32ptr;       // converted pointer to buffer
} log_t; 
extern volatile int p;
extern volatile log_t _log[];
#endif 

// WiFi stuff
// this will later be replaced by random values created in SETUP and proliferated via WPS
#define SSID "FRITZBox6660" // "WGK" // "FRITZBox6660" // "WGK" // 
#define PASS "gr9P.q7HZ.Lod9" // "start123"
#define WIFI_CHANNEL 8 

// UDP stuff
#define RX_IP_ADDR "192.168.20.3" // "192.168.4.1"  // 
#define PORT 45678

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#define MAX_RETRY 5

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#ifdef CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#endif

extern EventGroupHandle_t s_wifi_event_group;
void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// UDP stuff
extern uint8_t udpbuf[3 * NUM_SLOTS * NSAMPLES];

// I2S stuff
// The channel config is the same for both. 
static i2s_chan_config_t i2s_chan_cfg = {
    .id = I2S_NUM_AUTO,
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = DMA_BUFFER_COUNT,
    .dma_frame_num = NSAMPLES, 
    .auto_clear_after_cb = false, 
    .auto_clear_before_cb = false, 
    .allow_pd = false, 
    .intr_priority = 5, 
};    

// I2S Rx config for the sender
#ifdef I2S_STD
static i2s_std_config_t i2s_rx_cfg = {
#elif I2S_TDM
static i2s_tdm_config_t i2s_rx_cfg = {
#endif
    .clk_cfg = {                                // I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .sample_rate_hz = SAMPLE_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE,  // Set MCLK multiple to 256
        .clk_src = I2S_CLK_SRC_DEFAULT,         // we will use I2S_CLK_SRC_EXTERNAL !
        // .ext_clk_freq_hz = 11289600,         // wenn external. Muss sein sample_rate_hz * slot_bits * slot_num
    },
#ifdef I2S_STD
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH, I2S_SLOT_MODE_STEREO),
#elif I2S_TDM
    .slot_cfg = I2S_TDM_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH, I2S_SLOT_MODE_STEREO,
                                  I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3 ), 
//                              | I2S_TDM_SLOT4 | I2S_TDM_SLOT5 | I2S_TDM_SLOT6 | I2S_TDM_SLOT7 ),
#endif
    .gpio_cfg = {
        .mclk = GPIO_NUM_2,                    // pick the pins that are closest to the ADC without crossing PCB traces. 
        .bclk = GPIO_NUM_3,
        .ws = GPIO_NUM_4,
        .dout = I2S_GPIO_UNUSED,
        .din = GPIO_NUM_5,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = true,                     // we use AK5538 mode 32/36 (start on rising WS edge)
        },
    },
};

   // I2S Tx Config for the Receiver
#ifdef I2S_STD   
static i2s_std_config_t i2s_tx_cfg = {
#elif I2S_TDM
static i2s_tdm_config_t i2s_tx_cfg = {
#endif
    .clk_cfg = {                                // I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .sample_rate_hz = SAMPLE_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE,  // Set MCLK multiple to 256
        .clk_src = I2S_CLK_SRC_DEFAULT,         // we will use I2S_CLK_SRC_EXTERNAL !
        // .ext_clk_freq_hz = 11289600,         // wenn external. Muss sein sample_rate_hz * slot_bits * slot_num
    },
#ifdef I2S_STD
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH, I2S_SLOT_MODE_STEREO),
#elif I2S_TDM
    .slot_cfg = I2S_TDM_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH, I2S_SLOT_MODE_STEREO,
                                  I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3 ), 
//                              | I2S_TDM_SLOT4 | I2S_TDM_SLOT5 | I2S_TDM_SLOT6 | I2S_TDM_SLOT7 ),
#endif
    .gpio_cfg = {
        .mclk = GPIO_NUM_2,                    // pick the pins that are closest to the DAC without crossing PCB traces. 
        .bclk = GPIO_NUM_3,
        .ws = GPIO_NUM_4,
        .dout = GPIO_NUM_5,
        .din = I2S_GPIO_UNUSED,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = true,                     // we use AK4438 mode 14/18 (start on rising WS edge)
        },
    },
};



// Sender stuff
extern i2s_chan_handle_t i2s_rx_handle;
extern TaskHandle_t i2s_rx_task_handle; 
extern TaskHandle_t udp_tx_task_handle; 
void init_wifi_tx(void);
void udp_tx_task(void *pvParameters);
bool i2s_rx_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx); 
void i2s_rx_task(void *args);
bool init_gpio_tx(void);
void tx_setup (void);

// Receiver stuff
extern i2s_chan_handle_t i2s_tx_handle;
extern TaskHandle_t i2s_tx_task_handle; 
extern TaskHandle_t udp_rx_task_handle; 
void init_wifi_rx(void);
void udp_rx_task(void *pvParameters);
bool i2s_tx_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx); 
void i2s_tx_task(void *args);
bool init_gpio_rx(void);
void rx_setup (void);

// main stuff
typedef struct { 
    uint8_t *dma_buf;       // pointer to buffer
    uint32_t size;        // data size
} dma_params_t; 

uint32_t get_time_us_in_isr(void);

#define NEOPIX_PIN GPIO_NUM_27              // for the DevKit-C



#endif /* _WIRELESS_GK_H */

