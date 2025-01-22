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

// circular buffer library
// adapted from https://embeddedartistry.com/blog/2017/05/17/creating-a-circular-buffer-in-c-and-c/
// Copyright (C) Phillip Johnston, Creative Commons Zero v1.0 Universal

#include "wireless_gk.h"
#include "cbuf.h"

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
*/

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
        // to save two extra memcpys, we return only the pointer to the buffer tail
        // memcpy(data, me->buffer + (me->tail * me->elem_size), me->elem_size); 
        // i2s_tx_callback will then unpack directly using the pointer
        *data = me->buffer + (me->tail * me->elem_size); 
        retreat_pointer(me);

        r = me->elem_size;
    }

    return r;
}

