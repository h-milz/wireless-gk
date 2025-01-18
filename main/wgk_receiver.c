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

static esp_wps_config_t wps_config = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);

// uint8_t i2sbuf[SLOT_SIZE_I2S * NUM_SLOTS_I2S * NFRAMES];
// pointer to the last successfully received UDP datagram
static uint8_t *last_udp_buf = NULL; 

#ifdef RX_DEBUG
static char t[][20] = {"ISR", "begin i2s_tx loop", "after notify", "unpacking", "before recvfrom", "after recvfrom", "end i2s_tx loop" }; 
#endif
    

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data)
{
	switch (event_id) {
	case WIFI_EVENT_AP_START:
		ESP_LOGI(RX_TAG, "WIFI_EVENT_AP_START");
		break;
	case WIFI_EVENT_AP_STADISCONNECTED:
		{
			ESP_LOGI(RX_TAG, "WIFI_EVENT_AP_STADISCONNECTED");
			wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
			ESP_LOGI(RX_TAG, "station "MACSTR" leave, AID=%d, reason=%d",
					MAC2STR(event->mac), event->aid, event->reason);
		}
		break;
	case WIFI_EVENT_AP_STACONNECTED:
		{
			ESP_LOGI(RX_TAG, "WIFI_EVENT_AP_STACONNECTED");
			wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
			ESP_LOGI(RX_TAG, "station "MACSTR" join, AID=%d",
					MAC2STR(event->mac), event->aid);
		}
		break;
	case WIFI_EVENT_AP_WPS_RG_SUCCESS:
		{
			ESP_LOGI(RX_TAG, "WIFI_EVENT_AP_WPS_RG_SUCCESS");
			wifi_event_ap_wps_rg_success_t *evt = (wifi_event_ap_wps_rg_success_t *)event_data;
			ESP_LOGI(RX_TAG, "station "MACSTR" WPS successful",
					MAC2STR(evt->peer_macaddr));
		}
		break;
	case WIFI_EVENT_AP_WPS_RG_FAILED:
		{
			ESP_LOGI(RX_TAG, "WIFI_EVENT_AP_WPS_RG_FAILED");
			wifi_event_ap_wps_rg_fail_reason_t *evt = (wifi_event_ap_wps_rg_fail_reason_t *)event_data;
			ESP_LOGI(RX_TAG, "station "MACSTR" WPS failed, reason=%d",
						MAC2STR(evt->peer_macaddr), evt->reason);
			ESP_ERROR_CHECK(esp_wifi_ap_wps_disable());
			ESP_ERROR_CHECK(esp_wifi_ap_wps_enable(&wps_config));
			ESP_ERROR_CHECK(esp_wifi_ap_wps_start(NULL));
		}
		break;
	case WIFI_EVENT_AP_WPS_RG_TIMEOUT:
		{
			ESP_LOGI(RX_TAG, "WIFI_EVENT_AP_WPS_RG_TIMEOUT");
			ESP_ERROR_CHECK(esp_wifi_ap_wps_disable());
			ESP_ERROR_CHECK(esp_wifi_ap_wps_enable(&wps_config));
			ESP_ERROR_CHECK(esp_wifi_ap_wps_start(NULL));
		}
		break;
	default:
		break;
	}
}

/*
 * scan the network for the best free channel
 */ 

#define MAX_AP_RECORDS 20
#define CHAN_MAX 177
// https://en.wikipedia.org/wiki/List_of_WLAN_channels
static int channels[] = { 36, 40, 44, 48, 52, 56, 60, 64, 68, 96, 100, 104, 
                          108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 
                          149, 153, 157, 161, 165, 169, 173, 177 }; 
static int channel_usage[CHAN_MAX+1] = {0};  // For 5GHz use 
static wifi_ap_record_t ap_records[MAX_AP_RECORDS];

// TODO as soon as ESP-IDF supports country settings for 5G, use them, 
// preferably a safe mode that works internationally
// very possible that wifi_scan_config_t will be set correctly anyway. 
int find_free_channel(void) {
    // country settings? 
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,      // Scan all channels
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_PASSIVE,
    };

    // blink "channel scan"
    ESP_LOGI(RX_TAG, "performing channel scan");
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY));
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(RX_TAG, "Found %d access points", ap_count);

    if (ap_count > MAX_AP_RECORDS) {
        ap_count = MAX_AP_RECORDS;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));

    // Analyze channel usage
    for (int i = 0; i < ap_count; i++) {
        ESP_LOGI(RX_TAG, "SSID: %s, Channel: %d, RSSI: %d", ap_records[i].ssid, ap_records[i].primary, ap_records[i].rssi);
        channel_usage[ap_records[i].primary]++;
    }

    // Select the channel with the least congestion (least number of APs) 
    int best_channel = 0;
    int min_usage = INT_MAX;
    for (int i = 0; i < sizeof(channels)/sizeof(int); i++) {
        if (channel_usage[channels[i]] < min_usage) {
            best_channel = channels[i];
            min_usage = channel_usage[channels[i]];
        }
    }

    ESP_LOGI(RX_TAG, "Best channel: %d with usage count: %d", best_channel, min_usage);
    // esp_wifi_set_mode(WIFI_MODE_NULL); 
    ESP_ERROR_CHECK(esp_wifi_stop());
    esp_netif_destroy_default_wifi(WIFI_IF_STA);
    return (best_channel); 
}


#define MAXLEN 32

void init_wifi_rx(bool setup_requested) {
    // we are WiFi AP 

    char ssid[MAXLEN];
    char pass[MAXLEN];

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

#ifdef TX_TEST
    int channel = 100;
#else    
    int channel = find_free_channel(); 
#endif    
    ESP_LOGI(RX_TAG, "using free channel %d", channel);
    
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    wifi_config_t wifi_config; 
    int ret = esp_wifi_get_config(WIFI_IF_AP, &wifi_config);
    ESP_LOGI (RX_TAG, "esp_wifi_get_config found ssid %s", (char *)wifi_config.ap.ssid);
    ESP_LOGI (RX_TAG, "esp_wifi_get_config found pass %s", (char *)wifi_config.ap.password);
#ifdef TX_TEST
    if (true) {
#else        
    if (wifi_config.ap.ssid[0] == '\0' || ! strncmp ((char *)wifi_config.ap.ssid, "ESP_", 4)) {
#endif    
        // cfg is invalid.
#ifndef TX_TEST
        if (! setup_requested) {
            // we want the user to make an informed decision and not start WPS without consent
            ESP_LOGE(RX_TAG, "no valid WiFi credentials found - press setup!");
            // TODO blink "request setup"
            vTaskDelete(NULL);
        }
        ESP_LOGW(RX_TAG, "setup requested - creating new WiFi credentials");
#endif        
        memset(&wifi_config, 0, sizeof(wifi_config)); // Clear structure
#ifdef TX_TEST
        strncpy((char*)wifi_config.ap.ssid, SSID, sizeof(wifi_config.ap.ssid));
        strncpy((char*)wifi_config.ap.password, PASS, sizeof(wifi_config.ap.password));
        wifi_config.ap.ssid_len = strlen(SSID);
#else        
        // create random credentials.
        // TODO create SSID from SHA256 value
        snprintf (ssid, MAXLEN, "AP_%08lX%08lX%c", esp_random(), esp_random(), '\0');
        snprintf (pass, MAXLEN, "%08lX%08lX%c", esp_random(), esp_random(), '\0');
        ESP_LOGI(RX_TAG, "generated random ssid %s len %d pass %s", ssid, strlen(ssid), pass);
        strncpy((char*)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
        strncpy((char*)wifi_config.ap.password, pass, sizeof(wifi_config.ap.password));
        wifi_config.ap.ssid_len = strlen(ssid);
#endif
        wifi_config.ap.channel = channel;
        wifi_config.ap.max_connection = 2;          // TODO for debugging maybe, should be 1 in production
        wifi_config.ap.authmode = WIFI_AUTH_WPA3_PSK;
    }    
    
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_protocols_t proto_config = {0, };
    proto_config.ghz_2g = 0;
    proto_config.ghz_5g = WIFI_PROTOCOL_11AX;                    // we want 5 GHz 11AX WPA3. 

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP)); 
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));    

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY));
    ESP_ERROR_CHECK(esp_wifi_set_protocols(WIFI_IF_AP, &proto_config));            
    esp_wifi_set_ps(WIFI_PS_NONE);    // prevent  ENOMEM?             
    
    if (setup_requested) {
    	ESP_ERROR_CHECK(esp_wifi_ap_wps_enable(&wps_config));
  		ESP_ERROR_CHECK(esp_wifi_ap_wps_start(NULL));
  		ESP_LOGI(RX_TAG, "setup requested, enering wps ap registrar mode");
    }

    ESP_LOGI(RX_TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             wifi_config.ap.ssid, wifi_config.ap.password, channel);

}


// on_sent callback, used to determine the pointer to the most recently emptied dma buffer
IRAM_ATTR bool i2s_tx_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // we pass the *dma_buf and size in a struct, by reference. 
    static dma_params_t dma_params;
    dma_params.dma_buf = (uint8_t *)event->dma_buf;
    dma_params.size = (uint32_t)event->size;
#ifdef RX_DEBUG
    _log[p].loc = 0;
    _log[p].time = get_time_us_in_isr();
    _log[p].ptr = event->dma_buf;
    _log[p].size = event->size; 
    p++;
#endif
    xTaskNotifyFromISR(i2s_tx_task_handle, (uint32_t)(&dma_params), eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    return (xHigherPriorityTaskWoken == pdTRUE);
}    


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
        _log[p].loc = 1;
        _log[p].time = get_time_us_in_isr();
        p++;
#endif    
        // here we wait for the notify from the on_sent callback
        xTaskNotifyWait(0, ULONG_MAX, &evt_p, portMAX_DELAY);
        // convert back to pointer
        dma_params = (dma_params_t *)evt_p;    
        // and fetch pointer and size
        dmabuf = dma_params->dma_buf;
        size = dma_params->size;
        if (last_udp_buf == NULL) {
            ESP_LOGW(RX_TAG, "last_udp_buf = 0x%08lx", (uint32_t)last_udp_buf); 
            continue;  // can happen during the very first loop(s).         
        }
        
#ifdef RX_DEBUG        
        _log[p].loc = 2;
        _log[p].time = get_time_us_in_isr();
        _log[p].ptr = dmabuf;
        _log[p].size = size;
        p++;
#endif
        
        // checksum? should be checked in udp_rx_task, and leave last_udp_buf unchanged if checksum err. 
        
        // extract S1, S2
        // can be handled by a separate task running every, say, 10 ms. 

        // unpack and write to most recent free dma buffer
        // memset (dmabuf, 0, size);     // because .auto_clear_after_cb = true is set. 
        for (i=0; i<NFRAMES; i++) {
            for (j=NUM_SLOTS_I2S-1; j>=0; j--) {
                // the offset of a sample in the DMA buffer is (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S + 1
                // the offset of a sample in the UDP buffer is (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP
                 memcpy (dmabuf + (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S + 1, 
                         last_udp_buf + (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP, 
                         SLOT_SIZE_UDP); 
            }
        }
#ifdef LATENCY_MEAS        
        stop_time = get_time_us_in_isr();
#endif        
        
#ifdef RX_DEBUG        
        _log[p].loc = 3;
        _log[p].time = get_time_us_in_isr(); 
        _log[p].ptr = dmabuf;
        _log[p].size = size;
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
#ifdef RX_DEBUG        
        _log[p].loc = 6;
        _log[p].time = get_time_us_in_isr(); 
        p++;
#endif    
    }    
}


// udp_rx_task receives packets as they arrive, and puts them in udpbuf
// we use multiple udpbufs as a ring buffer to avoid race conditions and lost packets
#define NUM_UDP_BUFS_M_1 (NUM_UDP_BUFS - 1)
#define PACKETS_PER_SECOND (SAMPLE_RATE / NFRAMES)
void udp_rx_task(void *args) {

    int i, j;
    int k = 0;          // udpbuf ring buffer index
    uint32_t count, mycount = 0; 
    struct sockaddr_storage source_addr;
    socklen_t socklen = sizeof(source_addr);
    struct sockaddr_in dest_addr;
    struct timeval timeout;
    
    dest_addr.sin_addr.s_addr = inet_addr(RX_IP_ADDR); // htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    timeout.tv_sec = 1;       // 1 second timeout is plenty! 
    timeout.tv_usec = 0;

    last_udp_buf = udpbuf[0];    // in order to initialize a valid pointer address. 
    
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

            // k = (k+1) % NUM_UDP_BUFS; 
            // k = (k+1) >= NUM_UDP_BUFS ? 0 : k+1; // this way, we must add 1 twice. but it's faster than modulo division
            // this is the fastest variant if the number of buffers is a power of 2: 
            k = (k+1) & NUM_UDP_BUFS_M_1;
#ifdef RX_DEBUG        
            _log[p].loc = 4;
            _log[p].time = get_time_us_in_isr(); 
            p++;
#endif
            int len = recvfrom(sock, udpbuf[k], UDP_BUF_SIZE, 0, NULL, NULL); // (struct sockaddr *)&source_addr, &socklen);
#ifdef LATENCY_MEAS
            stop_time = get_time_us_in_isr();
            ESP_LOGI (RX_TAG, "latency: %lu µs", stop_time - start_time);
#endif

#ifdef RX_DEBUG        
            _log[p].loc = 5;
            _log[p].time = get_time_us_in_isr(); 
            _log[p].size = (uint32_t)len; 
            p++;
#endif

            // TODO checksum? should be checked in udp_rx_task, and leave last_udp_buf unchanged if checksum err. 
            // is a 8 bit XOR checksum enough?
                    
            if (len == UDP_BUF_SIZE) {
                // assume success. we copy the buffer pointer to last_udp_buf
                // otherwise, the previous successful datagram will be used. 
                last_udp_buf = udpbuf[k];
                mycount = (mycount + 1) & 0xFFFFFF;
                // display once a second
                if (mycount % PACKETS_PER_SECOND == 0) {
                    memcpy (&count, 
                            (uint32_t *)(last_udp_buf + (NUM_SLOTS_UDP-1) * SLOT_SIZE_UDP),
                            sizeof(uint32_t));
                    ESP_LOGI(RX_TAG, "count %lu, my_count %lu, diff %d", count, mycount, (int)count - (int)mycount); 
                }
            } else if (len == -1) {
                // we ignore broken packets for now. 
                ESP_LOGW(RX_TAG, "broken / lost packet");
                continue; 
            } else if (len < 0) {
                // TODO if we have a UDP timeout it's probably better to stop operation
                // to avoid artifacts from the loudspeakers. 
                ESP_LOGW(RX_TAG, "recvfrom failed: errno %d", errno);
                // blink "network failure, press reset"
                // vTaskDelete(NULL); 
                break;
            }
            
#ifdef RX_DEBUG        
            _log[p].loc = 4;
            _log[p].time = get_time_us_in_isr(); 
            p++;
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



