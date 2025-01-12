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

#include "wireless_gk.h"

#define LED_PIN                 GPIO_NUM_10             // 
#define SETUP_PIN               GPIO_NUM_14             // take the one that is nearest to the push button
#define SIG_PIN                 GPIO_NUM_8
#define ISR_PIN                GPIO_NUM_9


static const char *RX_TAG = "wgk_rx";

DRAM_ATTR volatile uint32_t start_time = 0;
static uint32_t stop_time = 0, diff_time = 0;

i2s_chan_handle_t i2s_tx_handle = NULL;
TaskHandle_t i2s_tx_task_handle = NULL; 
TaskHandle_t udp_rx_task_handle = NULL; 

uint8_t i2sbuf[SLOT_SIZE_I2S * NUM_SLOTS_I2S * NFRAMES];

IRAM_ATTR bool i2s_tx_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // we pass the *dma_buf and size in a struct, by reference. 
    static dma_params_t dma_params;
    dma_params.dma_buf = event->dma_buf;
    dma_params.size = event->size;
#ifdef RX_DEBUG
    _log[p].loc = 1;
    _log[p].time = get_time_us_in_isr();
    _log[p].ptr = event->dma_buf;
    _log[p].size = event->size; 
    p++;
#endif
    xTaskNotifyFromISR(i2s_tx_task_handle, (uint32_t)(&dma_params), eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    return (xHigherPriorityTaskWoken == pdTRUE);
}    

#ifdef RX_DEBUG
    char t[][15] = {"", "ISR", "begin loop", "after notify", "unpacking", "end loop", "recvfrom" }; 
#endif
    

void i2s_tx_task(void *args) {
    int i, j;
    uint32_t size;
    uint8_t *dmabuf;
    uint32_t loops = 0, led = 0;
    uint32_t evt_p;
    dma_params_t *dma_params;
    uint32_t nsamples = NFRAMES; // default
    
    // we get passed the *dma_buf and size, then pack, optionally checksum, and send. 
    while(1) {     
#ifdef RX_DEBUG
        _log[p].loc = 2;
        _log[p].time = get_time_us_in_isr();
        p++;
#endif    
        // here we wait for the notify from the on_sent callback
        // assume the udpbuf is available. 
        xTaskNotifyWait(0, ULONG_MAX, &evt_p, portMAX_DELAY);
        // convert back to pointer
        dma_params = (dma_params_t *)evt_p;    // könnte man sparen, wenn man eine union nehmen würde. 
        // and fetch pointer and size
        dmabuf = dma_params->dma_buf;
        size = dma_params->size;
#ifdef RX_DEBUG        
        _log[p].loc = 3;
        _log[p].time = get_time_us_in_isr();
        _log[p].ptr = dmabuf;
        _log[p].size = size;
        p++;
#endif

        // checksum? 
        
        // extract S1, S2

        // nsamples = size / (NUM_SLOTS * SLOT_WIDTH / 8);
        // ESP_LOGI(RX_TAG, "nsamples = %lu", nsamples);
        // could result in a race condition? if udpbuf is currently written to? 
        memset (dmabuf, 0, size);
        for (i=0; i<NFRAMES; i++) {
            for (j=NUM_SLOTS_I2S-1; j>=0; j--) {
                // the offset of a sample in the DMA buffer is (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S
                // the offset of a sample in the UDP buffer is (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP
                memcpy (dmabuf + (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S, 
                        udpbuf + (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP, 
                        SLOT_SIZE_UDP); 
            }
        }
        stop_time = get_time_us_in_isr();
        
        
#ifdef RX_DEBUG        
        _log[p].loc = 4;
        _log[p].time = get_time_us_in_isr(); 
        p++;
#endif    
        
            
        // ESP_LOGI(RX_TAG, "p = %d", p);
#ifdef RX_DEBUG
        if (p >= NUM) {
            i2s_channel_disable(i2s_tx_handle); // also stop all interrupts
            int q; 
            uint32_t curr_time, prev_time=0;
            for (q=0; q<p; q++) {
                switch (_log[q].loc) {
                    case 1:         // ISR
                        curr_time = _log[q].time;
                        printf("%15s time %lu diff %lu dma_buf 0x%08lx size %lu \n", 
                                t[_log[q].loc], _log[q].time, 
                                curr_time - prev_time,
                                (uint32_t) _log[q].ptr, _log[q].size);
                        prev_time = curr_time;
                        break; 
                    default:
                        printf("%15s time %lu diff %lu dma_buf 0x%08lx size %lu \n", 
                                t[_log[q].loc], _log[q].time, _log[q].time-_log[q-1].time, (uint32_t)_log[q].ptr, _log[q].size);
                        break;
                }                        
            }
            vTaskDelete(NULL); 
        }
#endif
        // blink LED
        loops = (loops + 1) % (SAMPLE_RATE / NFRAMES);
        if (loops == 0) {
            led = (led + 1) % 2;
            gpio_set_level(LED_PIN, led);
        }
    }    
}

static void wifi_ap_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(RX_TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(RX_TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

#define MAXLEN 32

void init_wifi_rx(bool setup_needed) {
    // we are WiFi AP 

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    wifi_config_t wifi_config; 
    int ret = esp_wifi_get_config(WIFI_IF_AP, &wifi_config);
    if (ret == ESP_OK && wifi_config.ap.ssid[0] != '\0' && false) {
        // cfg is valid.
        ESP_LOGI(RX_TAG, "Found saved Wi-Fi credentials: SSID: %s", wifi_config.sta.ssid);
    } else { 
        char ssid[MAXLEN];
        char pass[MAXLEN];
        // ESP_LOGW(TAG, "No saved AP configuration found. Using default.");
        memset(&wifi_config, 0, sizeof(wifi_config)); // Clear structure
        // we will later generate a random one in SETUP 
        snprintf (ssid, MAXLEN, "AP%08lX%08lX%c", esp_random(), esp_random(), '\0');
        snprintf (pass, MAXLEN, "%08lX%08lX%c", esp_random(), esp_random(), '\0');
        ESP_LOGI(RX_TAG, "generated random ssid %s pass %s", ssid, pass);
        strncpy((char*)wifi_config.ap.ssid, SSID, sizeof(wifi_config.ap.ssid));
        strncpy((char*)wifi_config.ap.password, PASS, sizeof(wifi_config.ap.password));
        wifi_config.ap.ssid_len = strlen(SSID);
        wifi_config.ap.channel = WIFI_CHANNEL;      // TODO can we scan the net and find free channels? 
        wifi_config.ap.max_connection = 2;          // TODO for debugging maybe, should be 1 in production
        // wifi_config.ap.ssid_hidden = 1;             // security by obscurity ;-) 
        wifi_config.ap.authmode = WIFI_AUTH_WPA3_PSK;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP)); 
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config) );    
    }         
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_ap_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_protocols_t proto_config = {0, };
    proto_config.ghz_2g = 0;
    proto_config.ghz_5g = WIFI_PROTOCOL_11AX;                    // we want 5 GHz 11AX WPA3. 

    ESP_ERROR_CHECK(esp_wifi_start() );
    ESP_ERROR_CHECK(esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY));
    ESP_ERROR_CHECK(esp_wifi_set_protocols(WIFI_IF_AP, &proto_config));            
    esp_wifi_set_ps(WIFI_PS_NONE);    // prevent  ENOMEM?             

    ESP_LOGI(RX_TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             wifi_config.ap.ssid, wifi_config.ap.password, WIFI_CHANNEL);

}


// udp_rx_task receives packets as they arrive, and puts them in udpbuf. 
// TODO do we need multiple udpbufs as a ring buffer? to avoid race conditions

void udp_rx_task(void *args) {

    int i, j;
    struct sockaddr_storage source_addr;
    socklen_t socklen = sizeof(source_addr);
    struct sockaddr_in dest_addr;
    struct timeval timeout;
    
    dest_addr.sin_addr.s_addr = inet_addr(RX_IP_ADDR); // htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
#ifdef RX_DEBUG
    char t[][15] = {"", "ISR", "begin loop", "after recvfrom", "unpacking", "i2s send", "end loop" }; 
#endif
    

    while (1) {

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(RX_TAG, "Unable to create socket: errno %d", errno);
            break;           // just try again. 
        }

        // Set timeout
        setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        
        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(RX_TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(RX_TAG, "Socket bound, port %d", PORT);

        while(1) {

            int len = recvfrom(sock, udpbuf, sizeof(udpbuf), 0, NULL, NULL); // (struct sockaddr *)&source_addr, &socklen);
#ifdef LATENCY_MEAS
            stop_time = get_time_us_in_isr();
            ESP_LOGI (RX_TAG, "latency: %lu µs", stop_time - start_time);
#endif

#ifdef RX_DEBUG        
            _log[p].loc = 6;
            _log[p].time = get_time_us_in_isr(); 
            _log[p].size = (uint32_t)len; 
            p++;
#endif
            // Error occurred during receiving
            if (len == -1) {
                continue; 
            } else if (len < 0) {
                ESP_LOGE(RX_TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            // we do nothing else here. the i2s_tx_task will be clocked by the on_sent events and pick up the udpbuf accordingly. 
            // we may need 2 bufs in order to avoid race conditions. 
            // ESP_LOGI (RX_TAG, "packet received, len %d", len);
            // unpack datagram
            // this way, we depend on the UDP jitter ... 
            // continue; 
            // EOT
            memset (i2sbuf, 0, sizeof(i2sbuf));
            for (i=0; i<NFRAMES; i++) {
                for (j=NUM_SLOTS_I2S-1; j>=0; j--) {
                    // the offset of a sample in the DMA buffer is (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S
                    // the offset of a sample in the UDP buffer is (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP
                    memcpy (i2sbuf + (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S, 
                            udpbuf + (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP, 
                            SLOT_SIZE_UDP); 
                }
            }
#ifdef RX_DEBUG        
            _log[p].loc = 4;
            _log[p].time = get_time_us_in_isr(); 
            p++;
#endif
            
            // and send
            i2s_channel_write(i2s_tx_handle, i2sbuf, sizeof(i2sbuf), NULL, 1000);
#ifdef RX_DEBUG        
            _log[p].loc = 5;
            _log[p].time = get_time_us_in_isr(); 
            p++;
            if (p >= NUM) {
                i2s_channel_disable(i2s_tx_handle); // also stop all interrupts
                int q; 
                uint32_t curr_time, prev_time=0;
                for (q=0; q<p; q++) {
                    switch (_log[q].loc) {
                        case 1:         // ISR
                            curr_time = _log[q].time;
                            printf("%15s time %lu diff %lu dma_buf 0x%08lx size %lu \n", 
                                    t[_log[q].loc], _log[q].time, 
                                    curr_time - prev_time,
                                    (uint32_t) _log[q].ptr, _log[q].size);
                            prev_time = curr_time;
                            break; 
                        default:
                            printf("%15s time %lu diff %lu dma_buf 0x%08lx size %lu \n", 
                                    t[_log[q].loc], _log[q].time, _log[q].time-_log[q-1].time, (uint32_t)_log[q].ptr, _log[q].size);
                            break;
                    }                        
                }
                // vTaskDelete(NULL); 
            }
            
#endif    
 
        }    
    }
}


#ifdef LATENCY_MEAS
// latency measurement
static void IRAM_ATTR handle_interrupt(void *args) {
    start_time = get_time_us_in_isr();
}
#endif


bool init_gpio_rx (void) {
    bool setup_needed = false; 
    
    // check if SETUP is pressed and then enter setup mode.  
    gpio_reset_pin(SETUP_PIN);
    gpio_set_direction(SETUP_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(SETUP_PIN, GPIO_PULLUP_ONLY);
    gpio_pullup_en(SETUP_PIN);
    if (gpio_get_level(SETUP_PIN) == 0) {
        setup_needed = true; 
    } 
    gpio_reset_pin(SETUP_PIN);

    // TODO Pins for S1 and S2
    // can be different for Rx and Tx depending on the PCB layout. 
    // can we read/write these using the ULP in the background every, say, 10 ms? 
            
    // LED to display function. 
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_pullup_dis(LED_PIN);
    gpio_set_intr_type(LED_PIN, GPIO_INTR_DISABLE);
    gpio_set_level(LED_PIN, 0);

#ifdef LATENCY_MEAS
    // enable the interrupt on ISR_PIN
    gpio_reset_pin(ISR_PIN); 
    gpio_set_direction(ISR_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(ISR_PIN, GPIO_PULLUP_ONLY);
    gpio_pullup_en(ISR_PIN);
    gpio_set_intr_type(ISR_PIN, GPIO_INTR_POSEDGE);
    // gpio_isr_register(handle_interrupt, NULL, ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM, NULL); 
    gpio_install_isr_service(0);
    gpio_isr_handler_add(ISR_PIN, handle_interrupt, NULL);
    gpio_intr_enable(ISR_PIN);
#endif

    // Returns true if setup was pressed
    return (setup_needed); 
}


void rx_setup (void) {
    // TODO start actual setup mode
    // on Rx, this means 
    // - generate new WiFi credentials
    // - store them in NVS
    // - start AP & WPS
}

