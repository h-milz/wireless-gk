/*
 * testbed for interpolating adjacent frame packets in the ring buffer
 * in case we need to duplicate a frame to mitigate packet loss, 
 * adjacent samples will likely see jumps at the crossover from packet(n-1) to packet(n). 
 * this code interpolates over the 5 frames at the joint in order to avoid jumps.
 * the frames are stored like this in the buffer:
 * ... | frame(n-1) | frame(n) || frame0 | frame1 | frame2 | .... 
 * so we use frame(n-1) and frame2 as the anchor points and interpolate the 3 frames in between, slot0 - slot6
 * using 5 frames with 4 gaps in between allows us to do simple integer math. 
 * we ignore slot7 because it contains GKVOL which is not critical.
 *
 * Mathematically, this method is suboptimal because although it will remove discontinuities, 
 * the chosen anchor points will most likely not be differentiable, i.e. bends.
 * However, the spectral content of these spots will cause much less audible glitches than real discontinuities.
 * At the end of the day, it's a tradeoff caused by the limited compute time. 
 *
 * Please check the Python demos in fft2.py and fft3.py in this directory to see the effect. 
 */ 



#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#define NUM_SLOTS_I2S 8
#define NFRAMES 5



typedef struct {
    int slot[NUM_SLOTS_I2S]; 
} i2s_frame_t;


i2s_frame_t frame[NFRAMES]; 

int i, j, l, step; 

struct timeval tv_start, tv_stop;
int loops = 1000000;


void main(void) {

    printf ("variant 1\n"); 

    // fill in random data at the anchor points, 0 otherwise
    for (j=0; j<NUM_SLOTS_I2S; j++) {
        frame[0].slot[j] = random(); 
        frame[1].slot[j] = frame[2].slot[j] = frame[3].slot[j] = 0; 
        frame[4].slot[j] = random(); 
    }
        
    // show buf
    for (i=0; i<NFRAMES; i++) {
        printf ("frame[%d] = ", i);
        for (j=0; j<NUM_SLOTS_I2S; j++) {
            printf ("%d ", frame[i].slot[j]);
        }
        printf ("\n");
    }


    gettimeofday(&tv_start,NULL);
    // i = slot index
    for (l=0; l<loops; l++) {
        for (i=0; i<NUM_SLOTS_I2S-1; i++) {     // we ignore slot7 which is GKVOL! 
            step = (frame[4].slot[i] - frame[0].slot[i]) / 4; 
            frame[1].slot[i] = frame[0].slot[i] + step;
            frame[2].slot[i] = frame[1].slot[i] + step;
            frame[3].slot[i] = frame[2].slot[i] + step; 
        }
    }
    gettimeofday(&tv_stop,NULL);
    printf ("variant 1: %lu µs for %d loops\n", (tv_stop.tv_sec*(uint64_t)1000000+tv_stop.tv_usec - 
                                                        (tv_start.tv_sec*(uint64_t)1000000+tv_start.tv_usec)), 
                                                        loops);

    // show buf
    for (i=0; i<NFRAMES; i++) {
        printf ("frame[%d] = ", i);
        for (j=0; j<NUM_SLOTS_I2S; j++) {
            printf ("%d ", frame[i].slot[j]);
        }
        printf ("\n");
    }

    printf ("variant 2\n"); 

    // fill in random data at the anchor points, 0 otherwise
    for (j=0; j<NUM_SLOTS_I2S; j++) {
        frame[0].slot[j] = random(); 
        frame[1].slot[j] = frame[2].slot[j] = frame[3].slot[j] = 0; 
        frame[4].slot[j] = random(); 
    }
        
    // show buf
    for (i=0; i<NFRAMES; i++) {
        printf ("frame[%d] = ", i);
        for (j=0; j<NUM_SLOTS_I2S; j++) {
            printf ("%d ", frame[i].slot[j]);
        }
        printf ("\n");
    }


    // on average, this version is ever so slightly faster on Linux
    gettimeofday(&tv_start,NULL);
    for (l=0; l<loops; l++) {
        for (i=0; i<NUM_SLOTS_I2S-1; i++) {     // we ignore slot7 which is GKVOL! 
            step = (frame[4].slot[i] - frame[0].slot[i]) / 4; 
            for (j=0; j<3; j++) {
                frame[j+1].slot[i] = frame[j].slot[i] + step;    
            }
        }
    }
    gettimeofday(&tv_stop,NULL);
    printf ("variant 2: %lu µs for %d loops\n", (tv_stop.tv_sec*(uint64_t)1000000+tv_stop.tv_usec - 
                                                        (tv_start.tv_sec*(uint64_t)1000000+tv_start.tv_usec)), 
                                                        loops); 

    // show buf
    for (i=0; i<NFRAMES; i++) {
        printf ("frame[%d] = ", i);
        for (j=0; j<NUM_SLOTS_I2S; j++) {
            printf ("%d ", frame[i].slot[j]);
        }
        printf ("\n");
    }


}
