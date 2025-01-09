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


static const char *TX_TAG = "wgk_tx";

i2s_chan_handle_t i2s_rx_handle = NULL;
TaskHandle_t i2s_rx_task_handle = NULL; 
TaskHandle_t udp_tx_task_handle = NULL; 

static int s_retry_num = 0;


IRAM_ATTR bool i2s_rx_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // raise signal pin for latency measurement
    gpio_set_level(SIG_PIN, 1);    

    // we pass the *dma_buf and size in a struct, by reference. 
    static dma_params_t dma_params;
    dma_params.dma_buf = event->dma_buf;
    dma_params.size = event->size;
#ifdef TX_DEBUG
    _log[p].loc = 1;
    _log[p].time = get_time_us_in_isr();
    _log[p].ptr = event->dma_buf;
    _log[p].size = event->size; 
    p++;
#endif
    // size is not used, so we could just transfer  (uint32_t)(event->dma_buf)
    xTaskNotifyFromISR(i2s_rx_task_handle, (uint32_t)(&dma_params), eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    return (xHigherPriorityTaskWoken == pdTRUE);
}    


void i2s_rx_task(void *args) {
    int i, j;
    uint32_t size;
    uint8_t *dmabuf;
    uint32_t loops = 0, led = 0;
    uint32_t evt_p;
    dma_params_t *dma_params;
    uint32_t nsamples = NFRAMES; // default
    
#ifdef TX_DEBUG
    char t[][15] = {"", "ISR", "begin loop", "after notify", "channel_read", "packing", "udp send", "end loop" }; 
#endif

    memset(udpbuf, 0, sizeof(udpbuf));    
    // we get passed the *dma_buf and size, then pack, optionally checksum, and send. 
    while(1) {     
#ifdef TX_DEBUG
        _log[p].loc = 2;
        _log[p].time = get_time_us_in_isr();
        p++;
#endif        
        xTaskNotifyWait(0, ULONG_MAX, &evt_p, portMAX_DELAY);
        // convert back to pointer
        dma_params = (dma_params_t *)evt_p;    
        // and fetch pointer and size
        dmabuf = dma_params->dma_buf;
        size = dma_params->size;                    // size is not used, so we could just transfer the *dma_buf. 
#ifdef TX_DEBUG        
        _log[p].loc = 3;
        _log[p].time = get_time_us_in_isr();
        _log[p].ptr = dmabuf;
        _log[p].size = size;
        p++;
#endif
        // read DMA buffer and pack
        memset (udpbuf, 0, sizeof(udpbuf));         // clean up first
        for (i=0; i<NFRAMES; i++) {
            for (j=0; j<NUM_SLOTS_I2S; j++) {                
                // the offset of a sample in the DMA buffer is (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S
                // the offset of a sample in the UDP buffer is (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP
                memcpy (udpbuf + (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP, 
                        dmabuf + (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S, 
                        SLOT_SIZE_UDP);
            }                    
        }
        // count the first sample
        // memcpy((uint32_t *)udpbuf, &loops, 4);
        udpbuf[0] = (loops & 0xff0000) >> 16;
        udpbuf[1] = (loops & 0xff00) >> 8;
        udpbuf[2] = (loops & 0xff);
        udpbuf[3] = 0xff;
        // udpbuf[size-1] = 0xff;
#ifdef TX_DEBUG        
        _log[p].loc = 5;
        _log[p].time = get_time_us_in_isr(); 
        p++;
#endif    
        // insert S1, S2
        
        // checksumming
        
        // send. 
        xTaskNotifyIndexed(udp_tx_task_handle, 0, size, eSetValueWithOverwrite);
            
        // release signal pin
        gpio_set_level(SIG_PIN, 0);    

        // ESP_LOGI(RX_TAG, "p = %d", p);
#ifdef TX_DEBUG
        if (p >= NUM) {
            i2s_channel_disable(i2s_rx_handle); // also stop all interrupts
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

static void wifi_sta_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TX_TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TX_TAG,"connect to the AP failed");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
/*
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK && ! ap_info.phy_11ax) {
            ESP_LOGW(TX_TAG, "Failed to connect using Wi-Fi 6 (802.11ax), retrying");
            esp_wifi_disconnect();
        } else {
*/
        ESP_LOGI(TX_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
//        }
    }
}


// Sender is WIFI_STA
void init_wifi_tx(void) {
    esp_err_t err; 
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config; 
    int ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
    if (ret == ESP_OK && wifi_config.sta.ssid[0] != '\0' && false) {
        // cfg is valid.
        ESP_LOGI(TX_TAG, "Found saved Wi-Fi credentials: SSID: %s", wifi_config.sta.ssid);
    } else {
        ESP_LOGW(TX_TAG, "No saved Wi-Fi credentials found. Using default.");
        memset(&wifi_config, 0, sizeof(wifi_config)); // Clear structure
        strncpy((char*)wifi_config.sta.ssid, SSID, sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, PASS, sizeof(wifi_config.sta.password));
        wifi_config.sta.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD;
        // wifi_config.sta.sae_pwe_h2e = ESP_WIFI_SAE_MODE,    // this is for WPA3
        // wifi_config.sta.sae_h2e_identifier = H2E_IDENTIFIER,
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // For STA mode
        // ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11AX));             // we want 11AX only 
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_sta_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_sta_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // esp_netif_t netif = esp_netif_get_default_netif();
    // esp_netif_set_hostname(netif, "wgk_sender");


/*
    err = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11AX);
    if (err != ESP_OK) {
        ESP_LOGW(TX_TAG, "esp_wifi_set_protocol returned err %d", err); 
    }
*/
    ESP_ERROR_CHECK(esp_wifi_start() );
    ESP_ERROR_CHECK(esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY));
    
    esp_wifi_set_ps(WIFI_PS_NONE);    // prevent  ENOMEM?
    
    ESP_LOGI(TX_TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);
    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TX_TAG, "connected to ap SSID:%s password:%s",
                 wifi_config.sta.ssid, wifi_config.sta.password);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGW(TX_TAG, "Failed to connect to SSID:%s, password:%s",
                 wifi_config.sta.ssid, wifi_config.sta.password);
    } else {
        ESP_LOGE(TX_TAG, "UNEXPECTED EVENT");
    }
}


/*
 * UDP stuff
 */
 

/* 
#include "esp_heap_trace.h"
#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS]; 
*/ 

// mostly copied from examples/protocols/sockets/udp_client/main/udp_client.c
void udp_tx_task(void *pvParameters) {
    struct sockaddr_in dest_addr;
    struct timeval timeout;
    int buf_size = 40960;  // UDP buffer size - adjust!
    int err; 

    dest_addr.sin_addr.s_addr = inet_addr(RX_IP_ADDR);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    // heap_trace_init_standalone(trace_record, NUM_RECORDS);
    // heap_trace_start(HEAP_TRACE_ALL);
    
    while (1) {

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TX_TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        // Set timeout
        setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        
        // TODO UDP send buffer size - default 5760, can we make much bigger! 
        // setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));

        ESP_LOGI(TX_TAG, "Socket created, sending to %s:%d", RX_IP_ADDR, PORT);

        while (1) {

            xTaskNotifyWait(0, ULONG_MAX, NULL, portMAX_DELAY);
            
            err = sendto(sock, udpbuf, sizeof(udpbuf), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            // ESP_LOGI(TX_TAG, "err=%d errno=%d", err, errno);
            if (err < 0) {
        	    if (errno == ENOMEM) {
        	        ESP_LOGW(TX_TAG, "lwip_sendto fail. %d", errno);
                    ESP_LOGI (TX_TAG, "largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        	        vTaskDelay(10);
        	    } else if (errno == 118) {
        	        ESP_LOGE(TX_TAG, "network not connected, errno %d", errno);
        	        // BLINK! 
        	        vTaskDelete(NULL);
                    break;
        	    } else {
        	        ESP_LOGE(TX_TAG, "lwip_sendto fail. %d", errno);
                    break;
                }
    	    }
            // ESP_LOGI(TX_TAG, "Message sent");

        }

        if (sock != -1) {
            ESP_LOGE(TX_TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
            // heap_trace_stop();
            // heap_trace_dump();
            // vTaskDelete(NULL);
        }
    }
    // should never be reached
    vTaskDelete(NULL);
}

bool init_gpio_tx (void) {
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
    // can we read these using the ULP in the background every, say, 10 ms? 
            
    // LED to display function. 
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_pullup_dis(LED_PIN);
    gpio_set_intr_type(LED_PIN, GPIO_INTR_DISABLE);
    gpio_set_level(LED_PIN, 0);
   
    // signal pin for loop measurements with oscilloscope
    gpio_reset_pin(SIG_PIN);
    gpio_set_direction(SIG_PIN, GPIO_MODE_OUTPUT);
    gpio_pullup_dis(SIG_PIN);
    gpio_set_intr_type(SIG_PIN, GPIO_INTR_DISABLE);
    gpio_set_level(SIG_PIN, 0);    

    // Returns true if setup was pressed
    return (setup_needed);
}


void tx_setup (void) {
    // TODO start actual setup mode
    // on Tx, this means 
    // - start STA & WPS
    // - store credentials in NVS


}


