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

#ifndef _CBUF_H
#define _CBUF_H

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

void circular_buf_reset(cbuf_handle_t me);
bool circular_buf_empty(cbuf_handle_t me); 
bool circular_buf_full(cbuf_handle_t me); 
cbuf_handle_t circular_buf_init(uint8_t *buffer, size_t size, size_t elem_size);
static void advance_pointer(cbuf_handle_t me); 
static void retreat_pointer(cbuf_handle_t me); 
void circular_buf_put(cbuf_handle_t me, uint8_t *data); 
size_t circular_buf_get(cbuf_handle_t me, uint8_t **data); 

#endif // CBUF_H 

