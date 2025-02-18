/**
 * @ Author: luoqi
 * @ Create Time: 2024-07-17 11:39
 * @ Modified by: luoqi
 * @ Modified time: 2025-02-18 22:12
 * @ Description:
 */

#ifndef _RINGBUF_H
#define _RINGBUF_H

#include <stdint.h>

#ifndef QNULL
#define QNULL ((void *)0)
#endif

typedef struct 
{
    uint32_t wr_index;    // ring buffer write index
    uint32_t rd_index;    // ring buffer read index
    uint32_t used;        // ring buffer used size
    uint32_t sz;          // ring buffer size
    uint8_t  *buf;
    int (*mutex_lock)(void);
    int (*mutex_unlock)(void);
} RingBuffer;

int rb_init(RingBuffer *rb, uint8_t *buf, uint32_t size, int (*mutex_lock)(void), int (*mutex_unlock)(void));

uint32_t rb_write(RingBuffer *rb, const uint8_t *data, uint32_t sz);

uint32_t rb_write_force(RingBuffer *rb, const uint8_t *data, uint32_t sz);

uint32_t rb_read(RingBuffer *rb, uint8_t *rdata, uint32_t sz);

uint32_t rb_used(RingBuffer *rb);

void rb_clr(RingBuffer *rb);

#endif
