/* 
 * simulate the receiving side UDP packet unpacking from 
 * 24 byte / sample to 32 byte / sample for AK4438 mode 18. 
 */ 

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
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "driver/pulse_cnt.h"
#include "rom/ets_sys.h"
#include "lwip/err.h"
#include <lwip/netdb.h>
#include "lwip/sockets.h"
#include "lwip/sys.h"


#define I2S_NUM                 I2S_NUM_AUTO
#define NSAMPLES                60                      // the number of samples we want to send in a batch
#define NUM_SLOTS               4                       // TDM256, 8 slots per sample
#define SLOT_WIDTH              32                      // 
#define DMA_BUFFER_COUNT        4                       // Number of DMA buffers. 
                                                        // 4 is enough because we pick up each individual one
#define DMA_BUFFER_SIZE         NSAMPLES * NUM_SLOTS * SLOT_WIDTH / 8  // Size of each DMA buffer

#define SAMPLE_RATE             44100                   // 44100
#define LED_PIN                 GPIO_NUM_15             // 
#define ID_PIN                  GPIO_NUM_23             // take the one that is nearest to GND
#define SETUP_PIN               GPIO_NUM_17             // take the one that is nearest to the push button
#define SIG_PIN                 GPIO_NUM_6
#define SIG2_PIN                GPIO_NUM_7

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

// #define RX_DEBUG 
// #define TX_DEBUG

static const char *TAG = "wirelessGK";
static const char *I2S_TAG = "wgk_i2s";
static const char *UDP_TAG = "wgk_udp";
static const char *WIFI_TAG = "wgk_wifi";
static const char *MON_TAG = "monitor_task";

// static volatile uint32_t sample_count = 0; // , txcount = 0, rxcount = 0, losses = 0, loopcount = 0, overall_losses = 0, overall_packets = 0;

// static uint8_t i2sbuf[4 * NUM_SLOTS * NSAMPLES];   // no longer needed, we work directly with the DMA bufs. 
static uint8_t udpbuf[3 * NUM_SLOTS * NSAMPLES];

// Sender
static i2s_chan_handle_t rx_handle = NULL;
static TaskHandle_t i2s_rx_task_handle; 
static TaskHandle_t udp_tx_task_handle; 

// Receiver
static i2s_chan_handle_t tx_handle = NULL;
static TaskHandle_t i2s_tx_task_handle; 
static TaskHandle_t udp_rx_task_handle; 


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
static i2s_tdm_config_t rx_cfg = {
    .clk_cfg = {                                // I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .sample_rate_hz = SAMPLE_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,  // Set MCLK multiple to 256
        .clk_src = I2S_CLK_SRC_DEFAULT,         // we will use I2S_CLK_SRC_EXTERNAL !
        // .ext_clk_freq_hz = 11289600,         // wenn external. Muss sein sample_rate_hz * slot_bits * slot_num
    },
    .slot_cfg = I2S_TDM_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO,
                                                I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3 ), 
//                                                | I2S_TDM_SLOT4 | I2S_TDM_SLOT5 | I2S_TDM_SLOT6 | I2S_TDM_SLOT7 ),
    .gpio_cfg = {
        .mclk = GPIO_NUM_22,                    // pick the pins that are closest to the ADC without crossing PCB traces. 
        .bclk = GPIO_NUM_4,
        .ws = GPIO_NUM_2,
        .dout = I2S_GPIO_UNUSED,
        .din = GPIO_NUM_21,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = true,                     // we use AK5538 mode 32/36 (start on rising WS edge)
        },
    },
};

#if 1
// I2S Tx Config for the Receiver
static i2s_std_config_t tx_cfg = {
    .clk_cfg = {                                // I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .sample_rate_hz = SAMPLE_RATE,
        .clk_src = I2S_CLK_SRC_DEFAULT,         // we will use I2S_CLK_SRC_EXTERNAL !
        .mclk_multiple = I2S_MCLK_MULTIPLE_256  // Set MCLK multiple to 256
    },
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
        .mclk = GPIO_NUM_22,                    // pick the pins that are closest to the DAC without crossing PCB traces. 
        .bclk = GPIO_NUM_4,
        .ws = GPIO_NUM_2,
        .dout = GPIO_NUM_5,
        .din = I2S_GPIO_UNUSED,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = true,                     // we use AK4438 mode 14/18 (start on rising WS edge)
        },
    },
};
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
DRAM_ATTR volatile int p = 0; 
DRAM_ATTR volatile log_t _log[NUM];   
#endif 

// Timer configuration
#include "driver/gptimer.h"

// Handle for the general-purpose timer
static gptimer_handle_t gptimer = NULL;

// Initialize the hardware timer
static void init_hardware_timer(void) {
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT, // Use default clock source
        .direction = GPTIMER_COUNT_UP,     // Count upwards
        .resolution_hz = 1000000,          // 1 MHz resolution (1 tick = 1 µs)
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
}

// ISR-safe function to read the timer value
static uint32_t get_time_us_in_isr(void) {
    uint64_t timer_val = 0;
    ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &timer_val));
    return (uint32_t) timer_val & 0xffffffff;
}


typedef struct { 
    uint8_t *dma_buf;       // pointer to buffer
    uint32_t size;        // data size
} dma_params_t; 


static IRAM_ATTR bool i2s_rx_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
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
    // TODO either pass the dma_buf as (uint32_t), or we do the packing here and send the index of the udp_buf. 
    xTaskNotifyFromISR(i2s_rx_task_handle, (uint32_t)(&dma_params), eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    return (xHigherPriorityTaskWoken == pdTRUE);
}    

static void i2s_rx_task(void *args) {
    int i;
    uint32_t size;
    uint8_t *dma_buf;
    uint32_t loops = 0, led = 0;
    uint32_t evt_p;
    dma_params_t *dma_params;
    uint32_t nsamples = NSAMPLES; // default
    
#ifdef RX_DEBUG
    char t[][15] = {"", "ISR", "begin loop", "after notify", "channel_read", "packing", "udp send", "end loop" }; 
#endif
    
    // we get passed the *dma_buf and size, then pack, optionally checksum, and send. 
    while(1) {     
#ifdef RX_DEBUG
        _log[p].loc = 2;
        _log[p].time = get_time_us_in_isr();
        p++;
#endif        
        xTaskNotifyWait(0, ULONG_MAX, &evt_p, portMAX_DELAY);
        // convert back to pointer
        dma_params = (dma_params_t *)evt_p;    // könnte man sparen, wenn man eine union nehmen würde. 
        // and fetch pointer and size
        dma_buf = dma_params->dma_buf;
        size = dma_params->size;
#ifdef RX_DEBUG        
        _log[p].loc = 3;
        _log[p].time = get_time_us_in_isr();
        _log[p].ptr = dma_buf;
        _log[p].size = size;
        p++;
#endif
        // read DMA buffer and pack
        // instead of using i2s_channel_read() which uses the FreeRTOS queue infrastructure internally 
        // and thus is not low latency capable, we access the last written DMA buffer directly. 
        // we can assume the passed data block is always a complete frame because
        // otherwise the DMA code would not have called the on_recv callback to begin with
        // but just in case it is less ... otherwise take NSAMPLES. 
        nsamples = size / (NUM_SLOTS * SLOT_WIDTH / 8);
        // ESP_LOGI(I2S_TAG, "nsamples = %lu", nsamples);
        for (i=0; i<nsamples; ++i) { 
            memcpy(udpbuf+3*i, dma_buf+4*i, 3); 
        }
        // count the first sample
        // memcpy((uint32_t *)udpbuf, &loops, 4);
        udpbuf[0] = (loops & 0xff0000) >> 16;
        udpbuf[1] = (loops & 0xff00) >> 8;
        udpbuf[2] = (loops & 0xff);
        udpbuf[3] = 0xff;
        udpbuf[size-1] = 0xff;
#ifdef RX_DEBUG        
        _log[p].loc = 5;
        _log[p].time = get_time_us_in_isr(); 
        p++;
#endif    
        // insert S1, S2
        
        // checksumming
        
        // send. 
        xTaskNotifyFromISR(udp_tx_task_handle, size, eSetValueWithOverwrite, NULL);
            
        // ESP_LOGI(I2S_TAG, "p = %d", p);
#ifdef RX_DEBUG
        if (p >= NUM) {
            i2s_channel_disable(rx_handle); // also stop all interrupts
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
        loops = (loops + 1) % (SAMPLE_RATE / NSAMPLES);
        if (loops == 0) {
            led = (led + 1) % 2;
            gpio_set_level(LED_PIN, led);
        }
    }    
}


/*
 * WiFi stuff
 */

// mostly copied from examples/wifi/getting_started/station/main/station_example_main.c
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

#define SSID "FRITZBox6660"
#define PASS "gr9P.q7HZ.Lod9"

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

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFI_TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


// Sender is WIFI_STA
static void init_wifi_sender(void) {

    s_wifi_event_group = xEventGroupCreate();
    esp_netif_t *netif;
    
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASS,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,

            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = H2E_IDENTIFIER,
        },
    };

    netif = esp_netif_get_default_netif();
    esp_netif_set_hostname(netif, "wgk_sender");

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    esp_wifi_set_ps(WIFI_PS_NONE);    // prevent  ENOMEM?
    
    ESP_LOGI(TAG, "wifi_init_sta finished.");

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
        ESP_LOGI(WIFI_TAG, "connected to ap SSID:%s password:%s",
                 SSID, PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s, password:%s",
                 SSID, PASS);
    } else {
        ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
    }
}


/*
 * UDP stuff
 */
 
#define HOST_IP_ADDR "192.168.20.3" 
#define PORT 45678

#include "esp_heap_trace.h"
#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS]; 

// mostly copied from examples/protocols/sockets/udp_client/main/udp_client.c
static void udp_tx_task(void *pvParameters) {
    struct sockaddr_in dest_addr;
    struct timeval timeout;
    int buf_size = 4096;  // UDP buffer size - adjust!
    int err; 

    dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    // heap_trace_init_standalone(trace_record, NUM_RECORDS);
    // heap_trace_start(HEAP_TRACE_ALL);
    
    while (1) {

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(UDP_TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        // Set timeout
        setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        
        // UDP send buffer size
        // setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));

        ESP_LOGI(TAG, "Socket created, sending to %s:%d", HOST_IP_ADDR, PORT);

        while (1) {

            xTaskNotifyWait(0, ULONG_MAX, NULL, portMAX_DELAY);
            
            err = sendto(sock, udpbuf, sizeof(udpbuf), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            // ESP_LOGI(UDP_TAG, "err=%d errno=%d", err, errno);
            if (err < 0) {
        	    if (errno == ENOMEM) {
        	        ESP_LOGW(UDP_TAG, "lwip_sendto fail. %d", errno);
                    ESP_LOGI (UDP_TAG, "largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        	        vTaskDelay(10);
        	    } else {
        	        ESP_LOGE(UDP_TAG, "lwip_sendto fail. %d", errno);
                    break;
                }
    	    }
            // ESP_LOGI(UDP_TAG, "Message sent");

#if 0
            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(UDP_TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
                if (strncmp(rx_buffer, "OK: ", 4) == 0) {
                    ESP_LOGI(TAG, "Received expected message, reconnecting");
                    break;
                }
            }

            vTaskDelay(2000 / portTICK_PERIOD_MS);
#endif
        }

        if (sock != -1) {
            ESP_LOGE(UDP_TAG, "Shutting down socket and restarting...");
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


static void monitor_task(void *pvParameters) {
    UBaseType_t hwm; 
    char *task_name; 
    TaskHandle_t myself = xTaskGetCurrentTaskHandle();
    TaskHandle_t tasks[] = { i2s_rx_task_handle, udp_tx_task_handle, myself };
    while (1) {
        for (int i=0; i<sizeof(tasks)/sizeof(TaskHandle_t); i++) {
            hwm = uxTaskGetStackHighWaterMark(tasks[i]);
            task_name = pcTaskGetName(tasks[i]);
            ESP_LOGI (MON_TAG, "task %s, HWM %d", task_name, hwm);
        }
        ESP_LOGI (MON_TAG, "largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

        vTaskDelay (1000/portTICK_PERIOD_MS);
    }
}


void app_main(void) {
    // int i, n;
    // int64_t start_time, stop_time;
    // float elapsed_time;
    bool setup = false, sender = false; 
    
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // check if we're sender or receiver. The sender has ID_PIN = 0
    gpio_reset_pin(ID_PIN);
    gpio_set_direction(ID_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(ID_PIN, GPIO_PULLUP_ONLY);
    gpio_pullup_en(ID_PIN);
    if (gpio_get_level(ID_PIN) == 0) {
        sender = true; 
    } 
    gpio_reset_pin(ID_PIN);

    // check if SETUP is pressed and then enter setup mode.  
    gpio_reset_pin(SETUP_PIN);
    gpio_set_direction(SETUP_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(SETUP_PIN, GPIO_PULLUP_ONLY);
    gpio_pullup_en(SETUP_PIN);
    if (gpio_get_level(SETUP_PIN) == 0) {
        setup = true; 
    } 
    gpio_reset_pin(SETUP_PIN);
    // TODO start actual setup mode

    // TODO Pins for S1 and S2
    // can be different for Rx and Tx depending on the PCB layout. 
    // can we read these using the ULP in the background every, say, 10 ms? 
            
    // LED to display function. 
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_pullup_dis(LED_PIN);
    gpio_set_intr_type(LED_PIN, GPIO_INTR_DISABLE);
    gpio_set_level(LED_PIN, 0);

/*    
    // signal pin for loop measurements with oscilloscope
    gpio_reset_pin(SIG_PIN);
    gpio_set_direction(SIG_PIN, GPIO_MODE_OUTPUT);
    gpio_pullup_dis(SIG_PIN);
    gpio_set_intr_type(SIG_PIN, GPIO_INTR_DISABLE);
    gpio_set_level(SIG_PIN, 0);    
*/

    init_hardware_timer();

    if (sender) {
        // initialize Wifi STA
        init_wifi_sender();
        
        // set up I2S receive channel on the Sender
        i2s_new_channel(&i2s_chan_cfg, NULL, &rx_handle);
        // this will later be i2s_channel_init_tdm_mode(). 
        i2s_channel_init_tdm_mode(rx_handle, &rx_cfg);

        // create I2S Rx task
        xTaskCreate(i2s_rx_task, "i2s_rx_task", 1024, NULL, 4, &i2s_rx_task_handle);
    
        // create UDP Tx task
        xTaskCreate(udp_tx_task, "udp_tx_task", 2048, NULL, 5, &udp_tx_task_handle);

        xTaskCreate(monitor_task, "monitor_task", 2048, NULL, 3, NULL);

        // create I2S rx on_recv callback
        i2s_event_callbacks_t cbs = {
            .on_recv = i2s_rx_callback,
            .on_recv_q_ovf = NULL,
            .on_sent = NULL,
            .on_send_q_ovf = NULL,
        };
        i2s_channel_register_event_callback(rx_handle, &cbs, NULL);

        // enable channel
        i2s_channel_enable(rx_handle);
        
    } else {
        // initialize Wifi AP and UDP listener
        // enable_network_receiver();
        
        // set up I2S send channel on the Receiver
        i2s_new_channel(&i2s_chan_cfg, &tx_handle, NULL);
        // this will later be i2s_channel_init_tdm_mode(). 
        i2s_channel_init_std_mode(tx_handle, &tx_cfg);
        // enable channel
        i2s_channel_enable(tx_handle);

        // hier könnte man die Steuerung über den on_sent callback machen. Wenn ein Buf geschickt ist, hole neues UDP-Paket. 
        // Dann braucht man da auch nicht zu pollen. 
        
        // xTaskCreate(udp_rx_task, "udp_rx_task", 4096, NULL, 5, NULL);
    }
    
    // we should never end up here. 
    // vTaskDelete(NULL);
    while (1) {
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
}


