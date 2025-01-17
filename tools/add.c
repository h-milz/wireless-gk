#include <string.h>
#include <stdlib.h>
#include <stdio.h> 
#include <stdint.h>
#include <sys/time.h>


#define LOOPS 100000007
#define NUM_UDP_BUFS 4
#define NUM_UDP_BUFS_M_1 (NUM_UDP_BUFS-1)

struct timeval tv_start, tv_stop;
int i, k;

void main(void) {

    k = 0;
    gettimeofday(&tv_start,NULL);
    for (i=0; i<LOOPS; i++) {
        k = (k+1) % NUM_UDP_BUFS;
    }
    gettimeofday(&tv_stop,NULL);
    printf ("variant 1: %lu µs for %d loops; k = %d\n", (tv_stop.tv_sec*(uint64_t)1000000+tv_stop.tv_usec - 
                                                        (tv_start.tv_sec*(uint64_t)1000000+tv_start.tv_usec)), 
                                                        LOOPS, 
                                                        k);
 
    k = 0;
    gettimeofday(&tv_start,NULL);
    for (i=0; i<LOOPS; i++) {
        k = (k+1) >= NUM_UDP_BUFS ? 0 : k+1;
    }
    gettimeofday(&tv_stop,NULL);
    printf ("variant 2: %lu µs for %d loops; k = %d\n", (tv_stop.tv_sec*(uint64_t)1000000+tv_stop.tv_usec - 
                                                        (tv_start.tv_sec*(uint64_t)1000000+tv_start.tv_usec)), 
                                                        LOOPS, 
                                                        k);
    k = 0;
    gettimeofday(&tv_start,NULL);
    for (i=0; i<LOOPS; i++) {
        k = (k+1) & ~NUM_UDP_BUFS; 
    }
    gettimeofday(&tv_stop,NULL);
    printf ("variant 3: %lu µs for %d loops; k = %d\n", (tv_stop.tv_sec*(uint64_t)1000000+tv_stop.tv_usec - 
                                                        (tv_start.tv_sec*(uint64_t)1000000+tv_start.tv_usec)), 
                                                        LOOPS, 
                                                        k);
    
    k = 0;
    gettimeofday(&tv_start,NULL);
    for (i=0; i<LOOPS; i++) {
        k = (k+1) & NUM_UDP_BUFS_M_1; 
    }
    gettimeofday(&tv_stop,NULL);
    printf ("variant 4: %lu µs for %d loops; k = %d\n", (tv_stop.tv_sec*(uint64_t)1000000+tv_stop.tv_usec - 
                                                        (tv_start.tv_sec*(uint64_t)1000000+tv_start.tv_usec)), 
                                                        LOOPS, 
                                                        k);
    


}
