

// cbuf nicht für udp_bufs sondern i2s_bufs, damit i2s_tx_callback nur abholen braucht. 
// an sich egal wie er kopiert, dauert etwa gleich lange. 
/*
 * if we have a udpbuf ring buffer, putting is slightly faster because it's less data
 * but getting means copying twice, first to an intermediate buffer, then unpacking to the DMA buffer. 
 * we can return the buffer pointer in _get. Then we can directly unpack from the returned pointer. 
 * but this is unclean as far as encapsulation. 
 * if we have an i2s ring buffer (unpacked), then putting will take slightly longer,
 * but we can _get directly to the DMA buffer. 
 * and this needs 4*1920 instead of 4*1440 byte
 */

#define NFRAMES                 60                      // the number of frames we want to send in a datagram
#define NUM_SLOTS_UDP           8                       // we always send 8 slot frames
#define SLOT_SIZE_UDP           3                       // UDP has 3-byte samples.
#define UDP_BUF_SIZE            NFRAMES * NUM_SLOTS_UDP * SLOT_SIZE_UDP
#define NUM_UDP_BUFS            4                       // must be a power of 2 due to the way the buffer index is incremented

// we build one large buffer and will access the individual parts using the element size. 
#include <stdint.h>
uint8_t *udpbuf;


// circular buffer library
// adapted from https://embeddedartistry.com/blog/2017/05/17/creating-a-circular-buffer-in-c-and-c/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>

struct circular_buf_t {
	size_t head;
	size_t tail;
	size_t max; //of the buffer
	size_t elem_size; 
	bool full;
	uint8_t *buffer;
};

typedef struct circular_buf_t circular_buf_t;
typedef circular_buf_t* cbuf_handle_t; 


void circular_buf_reset(cbuf_handle_t me) {
    assert(me);

    me->head = 0;
    me->tail = 0;
    me->full = false;
}


bool circular_buf_empty(cbuf_handle_t me) {
	assert(me);

	return (!me->full && (me->head == me->tail));
}


bool circular_buf_full(cbuf_handle_t me) {
	assert(me);

	return me->full;
}


cbuf_handle_t circular_buf_init(uint8_t *buffer, size_t size, size_t elem_size) {
	assert(buffer && size);

	cbuf_handle_t cbuf = malloc(sizeof(circular_buf_t));
	assert(cbuf);

	cbuf->buffer = buffer;
	cbuf->max = size;
	cbuf->elem_size = elem_size;
	circular_buf_reset(cbuf);

	assert(circular_buf_empty(cbuf));

	return cbuf;
}


// do we need this? 
/*
size_t circular_buf_capacity(cbuf_handle_t me) {
	assert(me);

	return me->max;
}
*/

size_t circular_buf_size(cbuf_handle_t me) {
	assert(me);

	size_t size = me->max;

	if(!me->full) {
		if(me->head >= me->tail) {
			size = (me->head - me->tail);
		} else {
			size = (me->max + me->head - me->tail);
		}
	}

	return size;
}


static void advance_pointer(cbuf_handle_t me) {
	assert(me);

	if(me->full) {
		if (++(me->tail) == me->max) { 
			me->tail = 0;
		}
	}

	if (++(me->head) == me->max) { 
		me->head = 0;
	}
	
	me->full = (me->head == me->tail);
}


static void retreat_pointer(cbuf_handle_t me) {
	assert(me);

	me->full = false;
	if (++(me->tail) == me->max) { 
		me->tail = 0;
	}
}


// this version of put will overwrite the oldest values
void circular_buf_put(cbuf_handle_t me, uint8_t *data) {
	assert(me && me->buffer);

    //me->buffer[me->head] = data;
    memcpy(me->buffer + (me->head * me->elem_size), data, me->elem_size); 

    advance_pointer(me);
}


size_t circular_buf_get(cbuf_handle_t me, uint8_t **data) {
    assert(me && data && me->buffer);

    // we return the number of bytes retrieved
    size_t r = 0;

    if(!circular_buf_empty(me)) {
        // *data = me->buffer[me->tail];
        // memcpy(data, me->buffer + (me->tail * me->elem_size), me->elem_size); 
        // to save one extra memcpy, we return only the pointer to the buffer tail        
        *data = me->buffer + (me->tail * me->elem_size); 
        retreat_pointer(me);

        r = me->elem_size;
    }

    return r;
}

void main(void) {
    uint8_t i, j, k; 
    
    // initialize udp buf
    udpbuf = (uint8_t *)malloc(NUM_UDP_BUFS * UDP_BUF_SIZE * sizeof(uint8_t)); 
    // fill in fake data - we want to see something
    for (i=0; i<NUM_UDP_BUFS; i++) {
        memset (udpbuf + i * UDP_BUF_SIZE, i, UDP_BUF_SIZE); 
    }

    // see what is in the buffer
    printf ("buffer content: %02d %02d %02d %02d \n", udpbuf[10], udpbuf[10+UDP_BUF_SIZE], 
                                                      udpbuf[10+2*UDP_BUF_SIZE], udpbuf[10+3*UDP_BUF_SIZE]);

    cbuf_handle_t cbuf = circular_buf_init(udpbuf, NUM_UDP_BUFS, UDP_BUF_SIZE); 
    
    // now let's play a bit
    // this is our received data after recv_from:
    uint8_t *recvbuf = (uint8_t *)calloc(UDP_BUF_SIZE, sizeof(uint8_t));
    // and this is our packed buf pointer for the i2s callback
    uint8_t *i2sbuf; //  = (uint8_t *)calloc(UDP_BUF_SIZE, sizeof(uint8_t));
    

    for (i=10; i<30; i++) {
        memset (recvbuf, i, UDP_BUF_SIZE);

        // put it in the buffer    
        circular_buf_put(cbuf, recvbuf);
        
        // see what is in the buffer
        printf ("cbuf content: %02d %02d %02d %02d \n", udpbuf[10], udpbuf[10+UDP_BUF_SIZE], 
                                                        udpbuf[10+2*UDP_BUF_SIZE], udpbuf[10+3*UDP_BUF_SIZE]);

        size_t res = circular_buf_get(cbuf, &i2sbuf);
        
        // see what is in i2sbuf
        printf ("i2sbuf content: %02d, res = %ld \n", i2sbuf[10], res);
        
        // sleep 200 ms
        // usleep(200000);
        
    
    
    
    
    }

    
    
        


}

