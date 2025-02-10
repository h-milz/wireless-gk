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
static uint32_t write_idx;             
static uint32_t idx_mask; 
static uint32_t ssn=1, rsn=1, prev_ssn;             // send_sequence_number, read_sequence_number, previous send_sequence_number
static uint32_t init_count = 0; 
static i2s_buf_t *ring_buf[NUM_RINGBUF_ELEMS];
static bool duplicated[NUM_RINGBUF_ELEMS];      // initialized to all zeroes = false
static const char *TAG = "wgk_ring_buf";
static uint32_t slot_mask = 0; 
static uint32_t all_mask = (1 << NUM_RINGBUF_ELEMS) - 1;   // e.g. 4 will result in 00001111
DRAM_ATTR static bool running = false;              // will be used in an ISR context
static uint32_t time2; 
DRAM_ATTR uint32_t time3 = 0;  
static bool done = false; 
static bool logging = true;                         // will be deactivated by the output routine
static uint32_t arr_time, last_arr_time = 0, diff_arr_time; 

#define SMOOTHE_SHORT 3
#define SMOOTHE_LONG 5
static i2s_frame_t *frame[SMOOTHE_LONG];    // just in case, works also when we use SHORT. 

#ifdef SSN_STATS
typedef struct {
    uint32_t timestamp;
    int sn;
    uint32_t bufssn;
} ssn_stat_t;
#define SSN_STAT_ENTRIES 8192
ssn_stat_t ssn_stat[SSN_STAT_ENTRIES];
uint32_t n = 0; 
#endif

#ifdef SSN_TRACE
DRAM_ATTR static uint32_t bufssn[NUM_RINGBUF_ELEMS]; 
#endif

// smoothe does a linear interpolation to remove discontinuities
// generated when we duplicate a packet to replace a missing packet
// see tools/interp.c for a discussion 
static void smoothe(i2s_buf_t *buf1, i2s_buf_t *buf2, int smooth_mode) {
    int step, i, j; 

    if (smooth_mode == SMOOTHE_LONG) {
        frame[0] = (i2s_frame_t *) buf1 + (NFRAMES-2)*sizeof(i2s_frame_t);   // these are the two last frames of the previous packet
        frame[1] = (i2s_frame_t *) buf1 + (NFRAMES-1)*sizeof(i2s_frame_t);
        frame[2] = (i2s_frame_t *) buf2;
        frame[3] = (i2s_frame_t *) buf2 + sizeof(i2s_frame_t);
        frame[4] = (i2s_frame_t *) buf2 + 2 * sizeof(i2s_frame_t);
        
        for (i=0; i<NUM_SLOTS_I2S-1; i++) {     // we ignore slot7 which is GKVOL!
            step = (frame[4]->slot[i] - frame[0]->slot[i]) / 4; 
            for (j=0; j<3; j++) {
                frame[j+1]->slot[i] = frame[j]->slot[i] + step;    
            }
        }
    } else if (smooth_mode == SMOOTHE_SHORT) {
        // easier: 
        // frame[0] = &(buf1->frame[NFRAMES-1]); 
        // frame[1] = &(buf2->frame[0]);
        // frame[2] = &(buf2->frame[1]); 
        frame[0] = (i2s_frame_t *) buf1 + (NFRAMES-1)*sizeof(i2s_frame_t);
        frame[1] = (i2s_frame_t *) buf2;
        frame[2] = (i2s_frame_t *) buf2 + sizeof(i2s_frame_t);
        
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
        // ESP_LOGI(TAG, "ringbuf[%d] = 0x%08x", i, (uint32_t)ring_buf[i]);
    }    
    idx_mask = NUM_RINGBUF_ELEMS - 1; 
    return true; 
}


// helper function
size_t ring_buf_size(void) {
    return NUM_RINGBUF_ELEMS * sizeof(i2s_buf_t); 
}


/*
 * duplicate packet to slot[idx2], smoothe and set dupe = true
 */
 
void duplicate (uint32_t idx1, uint32_t idx2) { 
    memcpy (ring_buf[idx2], ring_buf[idx1], sizeof(i2s_buf_t)); 
    smoothe (ring_buf[idx1], ring_buf[idx2], SMOOTHE_SHORT);
    duplicated[idx2] = true;
}


void ring_buf_put(udp_buf_t *udp_buf) {
    int i, j, d; 

    ssn = udp_buf->sequence_number;
#ifdef RX_STATS    
    arr_time = get_time_us_in_isr();
#endif
    
    if (ssn == prev_ssn + 1) {             // we're in the correct sequence
        if (ssn > rsn) {                   // this is a legitimate packet
            write_idx = ssn & idx_mask;     // no modulo, no if-else
            // unpack
            for (i=0; i<NFRAMES; i++) {
                for (j=NUM_SLOTS_I2S-1; j>=0; j--) {
                    // the offset of a sample in the DMA buffer is (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S + 1
                    // the offset of a sample in the UDP buffer is (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP
                    memcpy ((uint8_t *)ring_buf[write_idx] + (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S + 1, 
                            (uint8_t *)udp_buf + (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP, 
                            SLOT_SIZE_UDP); 
                }
            }
            // if the buffer on the left was duped -> smoothe. 
            if (duplicated[(ssn - 1) & idx_mask]) {
                smoothe (ring_buf[(ssn - 1) & idx_mask], ring_buf[write_idx], SMOOTHE_SHORT);
            }
            // this was a ligitimate packet, so we mark it accordingly. 
            duplicated[write_idx] = false; 
#ifdef SSN_TRACE            
            bufssn[write_idx] = ssn;
#endif            
            // duplicate the current packet to the next slot to mitigate errors in the next step
            duplicate (write_idx, (ssn + 1) & idx_mask); 
            // also smoothe the gap after the duplicate in case we see > 12 ms outages
            // this should avoid audible pops during a buffer overrun 
            smoothe (ring_buf[(ssn + 1) & idx_mask], 
                     ring_buf[(ssn + 2) & idx_mask], 
                     SMOOTHE_SHORT);
#ifdef SSN_TRACE            
            bufssn[(ssn + 1) & idx_mask] = ssn; 
#endif
            // duplicate twice? 
            // duplicate ((ssn + 1) & idx_mask, (ssn + 2) & idx_mask); 
            // and keep track of the sequencing
            init_count++;               // this needs only to be done when not running yet, 
                                        // but the ADDI is so fast that it makes no sense to check if running first. 
        }    
/*
 * these things never occured! 
    } else if (ssn < prev_ssn + 1) {
        // Packet is a duplicate (sent twice?) drop, ignore. 
        init_count = 0; 
    } else if (ssn > prev_ssn + 1) {
        // this can happen if the previous packet got lost entirely, i.e. was never received, and we now see the following one
        // fill intermediate slots with duplicates? 
        // we could just insert it into its dedicated slot and rely on the previously duplicated packet at ssn otherwise. 
        init_count = 0;
*/
    } 

#ifdef SSN_STATS
    if (logging) {
        ssn_stat[n].timestamp = get_time_us_in_isr();
        ssn_stat[n].sn = (int) ssn;
        ssn_stat[n].bufssn = (ssn > rsn) ? 1 : 0;      // we mark this as inserted if ssn > rsn else not
        n = (n+1) & 0x00001fff;       // ring 
    }
#endif


    // keep track of the sequencing
    prev_ssn = ssn; 
            
#ifdef RX_STATS            
    if (running) {
        diff_arr_time = arr_time - last_arr_time;
        last_arr_time = arr_time; 
        if (diff_arr_time > 4000) {
            ESP_LOGW(TAG, "%10lu", diff_arr_time);        
        }
    }
#endif    
    
                 
    if (!running && (init_count >= NUM_RINGBUF_ELEMS + RINGBUF_OFFSET)) {       // if we have enough consecutive valid packets: start replay. 
        running = true;
        time2 = get_time_us_in_isr(); 
        rsn = ssn - RINGBUF_OFFSET;                         // initial value. 
        // vTaskDelay (...); 
        // i2s_channel_enable(i2s_tx_handle);
        ESP_LOGI(TAG, "running"); 
        // TODO: LED "running" 
    }
    
    if (!done && running && (time3 != 0)) {
        // float quot = (float) (time3 - time2) / (1.0e6 * (float) NFRAMES / (float) SAMPLE_RATE);
        uint32_t diff = time3 - time2;
        ESP_LOGW(TAG, "--- time2 %lu time3 %lu diff %lu", time2, time3, diff); 
        done = true; 
    }

#ifdef RX_STATS 
    if (running) {
        d = ssn - rsn;
        if (d > stats[0]) stats[0] = d;
        if (d < stats[1]) stats[1] = d;
        if (ssn <= rsn)   stats[2]++;
    }
#endif            


    // for now, we ignore the checksum but sum up checksum errors for the statistics. 
#ifdef RX_STATS
    if ( udp_buf->checksum != calculate_checksum((uint32_t *)udp_buf, NFRAMES * sizeof(udp_frame_t) / 4) ) {
        // stats[6]++; 
    }     
#endif            

}


// This will be called in an ISR context so beware! 
IRAM_ATTR uint8_t *ring_buf_get(void) {
    uint8_t *p;

    if (running) {
#ifdef SSN_STATS
        if (logging) {
            ssn_stat[n].timestamp = get_time_us_in_isr();
            ssn_stat[n].sn = - (int) rsn;                   // negative to mark a get entry. 
            ssn_stat[n].bufssn = bufssn[rsn & idx_mask];    // ssn of the packet that is in the slot
            n = (n+1) & 0x00001fff;       // ring 
        }
#endif
        p = (uint8_t *)ring_buf[rsn & idx_mask];
        rsn = rsn + 1; 
        if (time3 == 0) {                   // when does the first fetch occur. 
            time3 = get_time_us_in_isr();
        }
        return p;
    } else {
        return NULL; 
    }
}


/* 
 * RX stats
 * 0 received
 * 1 ssn == prev_ssn + 1
 * 2 ssn <= rsn
 */
 
#ifdef RX_STATS
static char stats_msg[][20] = { "received", "ssn == prev_ssn + 1", "ssn <= rsn", }; 
                       
void rx_stats_task(void *args) {
    uint32_t overall_stats[NUM_STATS] = {0}; 
    uint32_t last_ssn = 0, packets_sent, overall_packets_sent = 0;
    int i; 
    uint32_t stat_count = 0; 
    char *TAG = "rx_stats";
    
    while (1) {
#if 0    
        packets_sent = ssn - last_ssn;
        last_ssn = ssn; 
        overall_packets_sent += packets_sent; 
        stat_count++; 
        printf ("%-10s %8lu : %10s %10s\n", "stats", stat_count, "intvl", "overall");  
        printf ("%-20s: %10lu %10lu\n", "packets sent", packets_sent, overall_packets_sent);
        for (i=0; i<NUM_STATS; i++) {
            overall_stats[i] += stats[i];
            printf ("%-20s: %10lu %10lu\n", stats_msg[i], stats[i], overall_stats[i]);
            stats[i] = 0;   
        }
        printf ("\n"); 
#endif 
        vTaskDelay(10000/ portTICK_PERIOD_MS); // every ~ 10 seconds 
        if (stats[2] > 0) {
            int delta_t, delta_t_ssn;
            uint32_t last_ts = 0, last_ssn_ts = 0;
            overall_stats[2] += stats[2]; 
            ESP_LOGI(TAG, "%10lu %10lu", stats[2], overall_stats[2]);
            stats[2] = 0; 

#ifdef SSN_STATS
            logging = false; 
            i2s_channel_disable(i2s_tx_handle);
            vTaskDelete(udp_rx_task_handle);
            vTaskDelay(1000/portTICK_PERIOD_MS);
            printf ("\nssn_stats\n");
            for (int m=0; m<SSN_STAT_ENTRIES; m++) {
                if (ssn_stat[m].sn > 0) {                                       // we have a put entry
                    delta_t = ssn_stat[m].timestamp - last_ts;
                    delta_t_ssn = ssn_stat[m].timestamp - last_ssn_ts;
                    last_ssn_ts = last_ts = ssn_stat[m].timestamp;
                    if (ssn_stat[m].bufssn) {                                   // packet was inserted
                        printf ("%10lu %10d ssn %10lu -> slot %lu and %lu\n", 
                                ssn_stat[m].timestamp,
                                delta_t_ssn,                                    // time since last valid packet
                                (uint32_t) ssn_stat[m].sn, 
                                (uint32_t) ssn_stat[m].sn & idx_mask,           // own slot
                                ((uint32_t) ssn_stat[m].sn + 1) & idx_mask      // next slot b/c duplicated
                                ); 
                    } else {                                                    // packet was logged but not inserted
                        printf ("%10lu %10d ssn %10lu\n", 
                                ssn_stat[m].timestamp,
                                delta_t_ssn,                                    // time since last valid packet
                                (uint32_t) ssn_stat[m].sn
                                );                     
                    }
                } else {                     // a get entry
                    delta_t = ssn_stat[m].timestamp - last_ts;
                    last_ts = ssn_stat[m].timestamp;
                    printf ("%10lu %10d rsn %10lu reads ssn %10lu from slot %lu\n", 
                            ssn_stat[m].timestamp,
                            delta_t,                                        // time since last event
                            (uint32_t) (- ssn_stat[m].sn),                  // the rsn
                            ssn_stat[m].bufssn,                             // ssn that is in the slot
                            (uint32_t) (- ssn_stat[m].sn) & idx_mask);      // own slot number
                
                }
            
            }

#endif  /* SSN_STATS */
        }        
        stat_count++;
        if (stat_count == 60) {         // after 10 minutes
            ESP_LOGW(TAG, "10 minutes %10lu mindiff %d maxdiff %d", overall_stats[2], stats[1], stats[0]);
        } 

    }
}
#endif  /* RX_STATS */

