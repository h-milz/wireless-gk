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

#ifndef _RINGBUF_H
#define _RINGBUF_H

#include "wireless_gk.h"

bool ring_buf_init(void);
size_t ring_buf_size(void); 
void ring_buf_put(udp_buf_t *udp_buf); 
IRAM_ATTR uint8_t *ring_buf_get(void);

#endif // _RINGBUF_H 

