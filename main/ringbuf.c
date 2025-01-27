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

#define SMOOTHE_SHORT 3
#define SMOOTHE_LONG 5
static i2s_frame_t *frame[SMOOTHE_LONG];    // just in case, works also when we use SHORT. 


// smoothe does a linear interpolation to remove discontinuities
// generated when we duplicate a packet to replace a missing packet
// see tools/interp.c for a discussion 
static void smoothe(i2s_buf_t *buf1, i2s_buf_t *buf2, int smooth_mode) {
    int step, i, j; 
    
    if (smooth_mode == SMOOTHE_LONG) {
        frame[0] = (i2s_frame_t *) (buf1 + (NFRAMES-2)*sizeof(i2s_frame_t));   // these are the two last frames of the previous packet
        frame[1] = (i2s_frame_t *) (buf1 + (NFRAMES-1)*sizeof(i2s_frame_t));
        frame[2] = (i2s_frame_t *) (buf2);
        frame[3] = (i2s_frame_t *) (buf2 + sizeof(i2s_frame_t));
        frame[4] = (i2s_frame_t *) (buf2 + 2 * sizeof(i2s_frame_t));
        
        for (i=0; i<NUM_SLOTS_I2S-1; i++) {     // we ignore slot7 which is GKVOL! 
            step = (frame[4]->slot[i] - frame[0]->slot[i]) / 4; 
            for (j=0; j<3; j++) {
                frame[j+1]->slot[i] = frame[j]->slot[i] + step;    
            }
        }
    } else if (smooth_mode == SMOOTHE_SHORT) {
        frame[0] = (i2s_frame_t *) (buf1 + (NFRAMES-1)*sizeof(i2s_frame_t));
        frame[1] = (i2s_frame_t *) (buf2);
        frame[2] = (i2s_frame_t *) (buf2 + sizeof(i2s_frame_t));
        
        for (i=0; i<NUM_SLOTS_I2S-1; i++) {     // we ignore slot7 which is GKVOL! 
            step = (frame[2]->slot[i] - frame[0]->slot[i]) / 2; 
            frame[1]->slot[i] = frame[0]->slot[i] + step;    
        }
    } else {
        // oops, this should not happen
    }
}


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
    bool need_smoothing = false; 
    
    write_idx = udp_buf->sequence_number & idx_mask;   // no modulo, no if-else
    
    // case decisions
    // TODO: we could try to adjust the read_idx offset if too many late packets are detected over time. 
    // i.e. decrease read_idx by one. 
    // in this case the whole insert/read would effectively slip by one. 
    // as an alternative to case #2, i.e. decrease read_idx first, then insert data
    // beware: in this case we would change read_idx in 2 locations and need locking. 
    // better: set bool slip_by_one=true and evaluate this in ring_buf_get
    // i.e. return ringbuf[read_idx-1] and do not increase read_idx. 
    // but it could happen that things normalize again and then we have effectively increased the latency by one packet. 
    if (write_idx >= ((read_idx + 1) & idx_mask)) {       // case #1: all's normal (+2) or late (+1). 
        // unpack, done. 
        for (i=0; i<NFRAMES; i++) {
            for (j=NUM_SLOTS_I2S-1; j>=0; j--) {
                // the offset of a sample in the DMA buffer is (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S + 1
                // the offset of a sample in the UDP buffer is (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP
                memcpy ((uint8_t *)ring_buf[write_idx] + (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S + 1, 
                        (uint8_t *)udp_buf + (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP, 
                        SLOT_SIZE_UDP); 
            }
        }
        if (!running) {
            slot_mask |= 1 << write_idx;                // take note of the filled slot
        }
        // TODO: can it happen that a correctly inserted packet does not match the previous one? 
        // if we had case #2 before, the current packet _should_ be the next regular one. 
        // otherwise we should smoothe here as well. 
    } else if (write_idx == read_idx) {                 // case #2: really late packet. drop, duplicate, and smoothe 
        // duplicate the packet at the current read position to the position one ahead
        uint32_t next_idx = (write_idx+1) & idx_mask; 
        memcpy ((uint8_t *)ring_buf[next_idx], 
                (uint8_t *)ring_buf[write_idx],
                sizeof(i2s_buf_t)); 
        smoothe (ring_buf[write_idx], ring_buf[next_idx], SMOOTHE_SHORT); 
        if (!running) {
            slot_mask |= 1 << next_idx;                 // take note of the filled slot
        }
    } else {                                            // case #3: really really late, we drop it altogether
    }
        
    // make sure the ring buffer gets filled before ring_buf_get returns != NULL
    // instead of counting we set bits corresponding to the slot numbers and wait until all bits are set
    // this makes sure that there is actual content in each of them.
    // then we set read_idx RINGBUF_OFFSET slots behind the last write_idx
    if (!running) {
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


