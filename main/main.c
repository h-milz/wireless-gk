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

#define ID_PIN                  GPIO_NUM_13             // take the one that is nearest to GND

static const char *TAG = "wgk_main";

EventGroupHandle_t s_wifi_event_group;

// static volatile uint32_t sample_count = 0; // , txcount = 0, rxcount = 0, losses = 0, loopcount = 0, overall_losses = 0, overall_packets = 0;

udp_buf_t *udp_tx_buf, *udp_rx_buf;
udp_frame_t *ringbuf;

#if (defined RX_DEBUG || defined TX_DEBUG)
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
uint32_t get_time_us_in_isr(void) {
    uint64_t timer_val = 0;
    ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &timer_val));
    return (uint32_t) timer_val & 0xffffffff;
}


/*
 * WiFi stuff
 */

// mostly copied from examples/wifi/getting_started/station/main/station_example_main.c
/* FreeRTOS event group to signal when we are connected*/
static int s_retry_num = 0;



void monitor_task(void *args) {
    UBaseType_t hwm; 
    char *task_name; 
    bool *sender = (bool *) args;
    TaskHandle_t tasks[] = { i2s_rx_task_handle, udp_tx_task_handle, i2s_tx_task_handle, udp_rx_task_handle, NULL };
    while (1) {
        for (int i=0; i<sizeof(tasks)/sizeof(TaskHandle_t); i++) {
            hwm = uxTaskGetStackHighWaterMark(tasks[i]);
            task_name = pcTaskGetName(tasks[i]);
            ESP_LOGI (TAG, "task %s, HWM %d", task_name, hwm);
        }
        ESP_LOGI (TAG, "largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        ESP_LOGI (TAG, "min free heap size: %lu", esp_get_minimum_free_heap_size());

        vTaskDelay (1000/portTICK_PERIOD_MS);
    }
}


// calculate simple XOR checksum based on uint32_t, which is much faster than using uint8_t
// there are 4x less XOR operations
// and ESP32 does not support unaligned uint8_t accesses and will always generate an exception
uint32_t calculate_checksum(uint32_t *buffer, size_t size) {
    uint32_t checksum = 0;
    int i; 
    
    for (i = 0; i < size; i++) {  
        checksum ^= *(buffer + i);
    }
    return checksum; 
}



void app_main(void) {

    bool sender = false; 
    bool creds_available = false;
    bool setup_requested = false;
    
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
      ESP_LOGW(TAG, "nvs erase, init"); 
    }
    ESP_ERROR_CHECK(ret);
    s_wifi_event_group = xEventGroupCreate();
    
    // check if we're sender or receiver. The sender has ID_PIN = 0
    gpio_reset_pin(ID_PIN);
    gpio_set_direction(ID_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(ID_PIN, GPIO_PULLUP_ONLY);
    gpio_pullup_en(ID_PIN);
    if (gpio_get_level(ID_PIN) == 0) {
        sender = true; 
    } 
    gpio_reset_pin(ID_PIN);

    init_hardware_timer();

    if (sender) {
        // initialize GPIO pins
        // returns true if setup was pressed
        setup_requested = init_gpio_tx();
        if (setup_requested) {   // setup needed
            ESP_ERROR_CHECK(nvs_flash_erase());
            nvs_flash_init();
            ESP_LOGI(TAG, "setup requested, entering setup mode");
        }
    
        // initialize Wifi STA
        init_wifi_tx(setup_requested);
        
        // let it settle. 
        vTaskDelay(200/portTICK_PERIOD_MS);
        
        // create udp buffers explicitly in RAM
        // we use only one in the sender. 
        udp_tx_buf = (udp_buf_t *)heap_caps_calloc(1, sizeof(udp_buf_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);         
        udp_tx_buf->sequence_number = 0;
        udp_tx_buf->switches = 0;
        
        // set up I2S receive channel on the Sender
        i2s_new_channel(&i2s_rx_chan_cfg, NULL, &i2s_rx_handle);
        // this will later be i2s_channel_init_tdm_mode(). 
#ifdef I2S_STD
        i2s_channel_init_std_mode(i2s_rx_handle, &i2s_rx_cfg);
#else
        i2s_channel_init_tdm_mode(i2s_rx_handle, &i2s_rx_cfg);
#endif        

// #ifdef LATENCY_MEAS            
        // xTaskCreate(latency_meas_task, "latency_meas_task", 4096, NULL, 5, NULL);
// #else        
        // create I2S Rx task
        // xTaskCreate(i2s_rx_task, "i2s_rx_task", 4096, NULL, 18, &i2s_rx_task_handle);
    
        // create UDP Tx task
        xTaskCreate(udp_tx_task, "udp_tx_task", 4096, NULL, 18, &udp_tx_task_handle);

        // xTaskCreate(monitor_task, "monitor_task", 4096, &sender, 3, NULL);

        // create I2S rx on_recv callback
        i2s_event_callbacks_t cbs = {
            .on_recv = i2s_rx_callback,
            .on_recv_q_ovf = NULL,
            .on_sent = NULL,
            .on_send_q_ovf = NULL,
        };
        i2s_channel_register_event_callback(i2s_rx_handle, &cbs, NULL);

        // enable channel
        i2s_channel_enable(i2s_rx_handle);
// #endif /* LATENCY_MEAS*/        
        
    } else {   // receiver
        // initialize GPIO pins
        // returns true if setup was pressed
        setup_requested = init_gpio_rx();
        if (setup_requested) {   // setup needed
            ESP_ERROR_CHECK(nvs_flash_erase());
            nvs_flash_init();
            ESP_LOGI(TAG, "setup requested, entering setup mode");
        }

        // TODO check if we had WiFi credentials in NVS, if not, setup. 
    
        // initialize Wifi AP
        init_wifi_rx(setup_requested);
        // let it settle. 
        vTaskDelay(200/portTICK_PERIOD_MS);
        
        // create udp ring buffer explicitly in DRAM
        ringbuf = (udp_frame_t *)heap_caps_calloc(NFRAMES * NUM_UDP_BUFS, sizeof(udp_frame_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL); 
        // and the UDP receive buffer
        udp_rx_buf = (udp_buf_t *)heap_caps_calloc(1, sizeof(udp_buf_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL); 
        udp_tx_buf->sequence_number = 0;
        udp_rx_buf->switches = 0;
            
        // set up I2S send channel on the Receiver
        i2s_new_channel(&i2s_tx_chan_cfg, &i2s_tx_handle, NULL);
        // this will later be i2s_channel_init_tdm_mode(). 
#ifdef I2S_STD
        i2s_channel_init_std_mode(i2s_tx_handle, &i2s_tx_cfg);
#else
        i2s_channel_init_tdm_mode(i2s_tx_handle, &i2s_tx_cfg);
#endif               

        // create UDP Rx task
        xTaskCreate(udp_rx_task, "udp_rx_task", 4096, NULL, 18, &udp_rx_task_handle);

        // xTaskCreate(monitor_task, "monitor_task", 4096, NULL, 3, NULL);

        // create I2S tx on_sent callback
        i2s_event_callbacks_t cbs = {
            .on_recv = NULL,
            .on_recv_q_ovf = NULL,
            .on_sent = i2s_tx_callback,
            .on_send_q_ovf = NULL,
        };
        i2s_channel_register_event_callback(i2s_tx_handle, &cbs, NULL);

        i2s_channel_enable(i2s_tx_handle);
    }
    
    // ESP_LOGW(TAG, "udp_buf_t = %d", sizeof(udp_buf_t));
    // ESP_LOGW(TAG, "UDP_PAYLOAD_SIZE = %d", UDP_PAYLOAD_SIZE);
    
    while (1) {
        vTaskDelay(1000/ portTICK_PERIOD_MS);
    }
}


