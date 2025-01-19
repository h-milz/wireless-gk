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

static esp_wps_config_t config = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);   // we support nothing else. 
static wifi_config_t wps_ap_creds[MAX_WPS_AP_CRED];
static int s_ap_creds_num = 0;
static int s_retry_num = 0;


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    static int ap_idx = 1;

    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TX_TAG, "WIFI_EVENT_STA_START");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TX_TAG, "WIFI_EVENT_STA_DISCONNECTED");
            if (s_retry_num < MAX_RETRY) {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI(TX_TAG, "retry to connect to the AP");
            } else if (ap_idx < s_ap_creds_num) {
                /* Try the next AP credential if first one fails */

                if (ap_idx < s_ap_creds_num) {
                    ESP_LOGI(TX_TAG, "Connecting to SSID: %s, Passphrase: %s",
                             wps_ap_creds[ap_idx].sta.ssid, wps_ap_creds[ap_idx].sta.password);
                    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wps_ap_creds[ap_idx++]) );
                    esp_wifi_connect();
                }
                s_retry_num = 0;
            } else {
                ESP_LOGI(TX_TAG, "Failed to connect!");
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }

            break;
        case WIFI_EVENT_STA_WPS_ER_SUCCESS:
            ESP_LOGI(TX_TAG, "WIFI_EVENT_STA_WPS_ER_SUCCESS");
            {
                wifi_event_sta_wps_er_success_t *evt =
                    (wifi_event_sta_wps_er_success_t *)event_data;
                int i;

                if (evt) {
                    s_ap_creds_num = evt->ap_cred_cnt;
                    for (i = 0; i < s_ap_creds_num; i++) {
                        memcpy(wps_ap_creds[i].sta.ssid, evt->ap_cred[i].ssid,
                               sizeof(evt->ap_cred[i].ssid));
                        memcpy(wps_ap_creds[i].sta.password, evt->ap_cred[i].passphrase,
                               sizeof(evt->ap_cred[i].passphrase));
                    }
                    /* If multiple AP credentials are received from WPS, connect with first one */
                    ESP_LOGI(TX_TAG, "Connecting to SSID: %s, Passphrase: %s",
                             wps_ap_creds[0].sta.ssid, wps_ap_creds[0].sta.password);
                    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wps_ap_creds[0]) );
                }
                /*
                 * If only one AP credential is received from WPS, there will be no event data and
                 * esp_wifi_set_config() is already called by WPS modules for backward compatibility
                 * with legacy apps. So directly attempt connection here.
                 */
                ESP_ERROR_CHECK(esp_wifi_wps_disable());
                // esp_wifi_connect(); 
                esp_restart();    // because the wifi_connect() does not return to the originally calling routine
            }
            break;
        case WIFI_EVENT_STA_WPS_ER_FAILED:
            ESP_LOGI(TX_TAG, "WIFI_EVENT_STA_WPS_ER_FAILED");
            ESP_ERROR_CHECK(esp_wifi_wps_disable());
            ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
            ESP_ERROR_CHECK(esp_wifi_wps_start(0));
            break;
        case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
            ESP_LOGI(TX_TAG, "WIFI_EVENT_STA_WPS_ER_TIMEOUT");
            ESP_ERROR_CHECK(esp_wifi_wps_disable());
            ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
            ESP_ERROR_CHECK(esp_wifi_wps_start(0));
            break;
        default:
            break;
    }
}


static void got_ip_event_handler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TX_TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
}


// Sender is WIFI_STA
void init_wifi_tx(bool setup_requested) {
    esp_err_t err; 
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_event_handler, NULL));

#ifdef TX_TEST
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID, 
            .password = PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_WPA3_PSK,
        }
    }; 
#else 
    wifi_config_t wifi_config; 

    int ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
    ESP_LOGI (TX_TAG, "esp_wifi_get_config found ssid %s", (char *)wifi_config.sta.ssid);
    ESP_LOGI (TX_TAG, "esp_wifi_get_config found pass %s", (char *)wifi_config.sta.password);
    if (ret != ESP_OK || wifi_config.sta.ssid[0] == '\0') {
        // no valid config
        if (! setup_requested) {
            // we want the user to make an informed decision and not start WPS without consent
            ESP_LOGE(TX_TAG, "no valid WiFi credentials found - press setup!");
            // TODO blink "request setup"
            vTaskDelete(NULL);
        } 
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA3_PSK;                                // not more, not less.     
#endif
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY));

    if (setup_requested) {
        ESP_LOGI(TX_TAG, "setup requested, starting wps...");
        // TODO blink "starting wps" 
        ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));    
    } else {
        ESP_LOGI(TX_TAG, "normal STA startup...");
    }        
    
    esp_wifi_set_ps(WIFI_PS_NONE);    // prevent  ENOMEM?
    
    // ESP_LOGI(TX_TAG, "wifi_init_sta finished.");

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
        int ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
        ESP_LOGI(TX_TAG, "connected to ap SSID:%s password:%s",
                 wifi_config.sta.ssid, wifi_config.sta.password);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGW(TX_TAG, "Failed to connect to SSID:%s, password:%s",
                 wifi_config.sta.ssid, wifi_config.sta.password);
    } else {
        ESP_LOGE(TX_TAG, "UNEXPECTED EVENT");
    }
}


IRAM_ATTR bool i2s_rx_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
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
    uint32_t loops = 0, led = 0x00;
    uint32_t evt_p;
    dma_params_t *dma_params;
    uint32_t count = 0; 
    uint8_t checksum; 
    uint32_t num_bytes = UDP_BUF_SIZE - 2 * NUM_SLOTS_UDP * SLOT_SIZE_UDP;


    
#ifdef TX_DEBUG
    char t[][15] = {"", "ISR", "begin loop", "after notify", "channel_read", "packing", "udp send", "end loop" }; 
#endif

    memset(udpbuf[0], 0, UDP_BUF_SIZE);    
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
#ifndef TX_TEST
        memcpy (udpbuf[0], dmabuf, size);         
#else        
        // memset (udpbuf[0], 0, UDP_BUF_SIZE);         // clean up first
        for (i=0; i<NFRAMES; i++) {
            for (j=0; j<NUM_SLOTS_I2S; j++) {                
                // the offset of a sample in the DMA buffer is (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S + 1 
                // the offset of a sample in the UDP buffer is (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP
                memcpy (udpbuf[0] + (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP, 
                        dmabuf + (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S + 1,             // bytes are big endian. 
                        SLOT_SIZE_UDP);
            }                    
        }
#endif
        // insert current packet count into the last slot. only 24 least significant bits
        count = (count + 1) & 0x00FFFFFF; 
        memcpy ((uint32_t *)(udpbuf[0] + UDP_BUF_SIZE - 3 ),      
                &count,
                3); 
                
        // insert XOR checksum 
        // into second byte of the last slot
        checksum = 0; 
        for (i = 0; i < num_bytes; i++) {
            checksum ^= udpbuf[0][i];
        }
        udpbuf[0][UDP_BUF_SIZE - 27] = checksum;    // one slot before. 


#ifdef TX_DEBUG        
        _log[p].loc = 5;
        _log[p].time = get_time_us_in_isr(); 
        p++;
#endif    
        // insert S1, S2
        
        // checksumming
        
        // send. 
        xTaskNotifyIndexed(udp_tx_task_handle, 0, size, eSetValueWithOverwrite);

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
/*        
        loops = (loops + 1) % (SAMPLE_RATE / NFRAMES);
        if (loops == 0) {
            // led ^= 0x01; // toggle
            // gpio_set_level(LED_PIN, led);
            for (i=0; i<NUM_SLOTS_I2S; i++) {
                for (j=0; j<SLOT_SIZE_I2S; j++) {
                    printf ("%02x", dmabuf[i * SLOT_SIZE_I2S +j]);   
                }                    
                printf (" ");             
            }
            printf ("\n");
        }
*/        
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
void udp_tx_task(void *args) {
    struct sockaddr_in dest_addr;
    struct timeval timeout;

    int err; 

    dest_addr.sin_addr.s_addr = inet_addr(RX_IP_ADDR);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

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
            
            err = sendto(sock, udpbuf[0], UDP_BUF_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            // ESP_LOGI(TX_TAG, "err=%d errno=%d", err, errno);
            if (err < 0) {
        	    if (errno == ENOMEM) {
        	        ESP_LOGW(TX_TAG, "lwip_sendto fail ENOMEM. %d", errno);
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

#ifdef LATENCY_MEAS            
void latency_meas_task(void *args) {
    struct sockaddr_in dest_addr;
    struct timeval timeout;
    int err; 

    dest_addr.sin_addr.s_addr = inet_addr(RX_IP_ADDR);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TX_TAG, "Unable to create socket: errno %d", errno);
    }

    // Set timeout
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // TODO UDP send buffer size - default 5760, can we make much bigger! 
    // setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));

    ESP_LOGI(TX_TAG, "Socket created, sending to %s:%d", RX_IP_ADDR, PORT);

    memset (udpbuf[0], 0xaa, UDP_BUF_SIZE);         // fake data

    while (1) {
        gpio_set_level(SIG_PIN, 1);    

        err = sendto(sock, udpbuf[0], UDP_BUF_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        
        ESP_LOGI (TX_TAG, "latency packet sent");

        // release signal pin
        gpio_set_level(SIG_PIN, 0);    
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}
#endif




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


