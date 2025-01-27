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

// in fact, this is not actually a ring buffer as far as the writes 
// because data is written to the buffer number that corresponds with its sequence number
// but the ring buffer is read sequentially after all, so ... 

#include "wireless_gk.h"

// using uint32_t for small values looks like a waste of memory but 
// a) we have plenty of RAM and 
// b) ESP32 cannot handle non-32bit-aligned variables like uint8_t very well
// so using a 32-bit variable is in fact more run-time efficient 
DRAM_ATTR static uint32_t read_idx = 0;  
static uint32_t write_idx;             
static uint32_t idx_mask; 
static i2s_buf_t *ring_buf[NUM_RINGBUF_ELEMS];
static const char *TAG = "wgk_ring_buf";
static uint32_t slot_mask = 0; 
static uint32_t all_mask = (1 << NUM_RINGBUF_ELEMS) - 1;   // e.g. 4 will result in 00001111
DRAM_ATTR static bool running = false;              // will be used in an ISR context


bool ring_buf_init(void) {
    int i; 
    for (i=0; i<NUM_RINGBUF_ELEMS; i++) {
        // calloc ring_buffers explicitly in internal RAM
        ring_buf[i] = (i2s_buf_t *)heap_caps_calloc(1, sizeof(i2s_buf_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL); 
        if (ring_buf[i] == NULL) {               // This Should Not Happen[TM]
            ESP_LOGE(TAG, "%s: calloc failed: errno %d", __func__, errno); 
            return false; 
        }
    }    
    idx_mask = NUM_RINGBUF_ELEMS - 1; 
    return true; 
}


// helper function
size_t ring_buf_size(void) {
    return NUM_RINGBUF_ELEMS * sizeof(i2s_buf_t); 
}


void ring_buf_put(udp_buf_t *udp_buf) {
    int i, j; 
    write_idx = udp_buf->sequence_number & idx_mask;   // no modulo, no if-else

    // here we need the case decisions
    
    // here we do the unpack. 
    for (i=0; i<NFRAMES; i++) {
        for (j=NUM_SLOTS_I2S-1; j>=0; j--) {
            // the offset of a sample in the DMA buffer is (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S + 1
            // the offset of a sample in the UDP buffer is (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP
            memcpy ((uint8_t *)ring_buf[write_idx] + (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S + 1, 
                    (uint8_t* )udp_buf + (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP, 
                    SLOT_SIZE_UDP); 
        }
    }

    
    // here we will call smoothing if required. 
    
    // make sure the ring buffer gets filled before ring_buf_get returns != NULL
    // instead of counting we set bits corresponding to the slot numbers and wait until all bits are set
    // this makes sure that there is actual content in each of them.
    // then we set read_idx RINGBUF_OFFSET slots behind the last write_idx
    if (!running) {
        slot_mask |= 1 << write_idx; 
        if (slot_mask == all_mask) {    // all bits are set so we're full
            running = true;
            read_idx = (write_idx - RINGBUF_OFFSET) & idx_mask; 
            ESP_LOGI(TAG, "running"); 
        }
    }
}


// This will be called in an ISR context so beware! 
IRAM_ATTR uint8_t *ring_buf_get(void) {
    uint8_t *p;
    if (running) {
        p = (uint8_t *)ring_buf[read_idx]; 
        read_idx = (read_idx + 1) & idx_mask;   // avoid if-else or modulo stuff. 
        return p;
    } else {
        return NULL; 
    }
}


