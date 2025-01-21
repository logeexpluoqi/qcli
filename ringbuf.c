/**
 * @ Author: luoqi
 * @ Create Time: 2024-07-17 11:39
 * @ Modified by: luoqi
 * @ Modified time: 2025-01-21 20:52
 * @ Description:
 */

#include "ringbuf.h"

#define _RB_NULL  ((void *)0)
#define _RB_IS_VALID(rb)  (rb != _RB_NULL)

static inline void *_memcpy(void *dst, const void *src, uint32_t sz)
{
    char *d = (char *)dst;
    const char *s = (const char *)src;

    if (d < s) {
        while (sz--) {
            *d++ = *s++;
        }
    } else {
        d += sz;
        s += sz;
        while (sz--) {
            *--d = *--s;
        }
    }
    return dst;
}

int rb_init(RingBuffer *rb, uint8_t *buf, uint32_t size)
{
    if(!_RB_IS_VALID(rb) || !_RB_IS_VALID(buf)) {
        return -1;
    }
    rb->rd_index = 0;
    rb->wr_index = 0;
    rb->used = 0;
    rb->sz = size;
    rb->buf = buf;
    return 0;
}

uint32_t rb_write_force(RingBuffer *rb, const uint8_t *data, uint32_t sz)
{
    if(!_RB_IS_VALID(rb) || !_RB_IS_VALID(data)) {
        return -1;
    }
    uint32_t wsz = 0;                               // write size
    uint32_t wtimes = (sz + rb->sz - 1) / rb->sz;   // write times
    uint32_t wdsz = 0;                              // writed size

    for(uint32_t i = 0; i < wtimes; i++) {
        wsz = (sz > rb->sz) ? rb->sz : sz;
        if(rb->wr_index + wsz > rb->sz) {
            _memcpy(rb->buf + rb->wr_index, data + wdsz, rb->sz - rb->wr_index);
            wdsz = wdsz + (rb->sz - rb->wr_index);
            _memcpy(rb->buf, data + wdsz, wsz - (rb->sz - rb->wr_index));
            wdsz = wdsz + (wsz - (rb->sz - rb->wr_index));
        } else {
            _memcpy(rb->buf + rb->wr_index, data + wdsz, wsz);
            wdsz = wdsz + wsz;
        }
        sz = sz - wsz;
        rb->wr_index = (rb->wr_index + wsz) % rb->sz;
    }
    rb->used = (rb->used + wdsz) > rb->sz ? rb->sz : (rb->used + wdsz);

    return wdsz;
}

uint32_t rb_write(RingBuffer *rb, const uint8_t *data, uint32_t sz)
{
    if(!_RB_IS_VALID(rb) || !_RB_IS_VALID(data)) {
        return 0;
    }
    sz = sz > (rb->sz - rb->used) ? (rb->sz - rb->used) : sz;
    return rb_write_force(rb, data, sz);
}

uint32_t rb_read(RingBuffer *rb, uint8_t *rdata, uint32_t sz)
{
    if(!_RB_IS_VALID(rb) || !_RB_IS_VALID(rdata)) {
        return 0;
    }
    sz = (sz > rb->used) ? rb->used : sz;

    if(rb->rd_index + sz > rb->sz) {
        _memcpy(rdata, rb->buf + rb->rd_index, rb->sz - rb->rd_index);
        _memcpy(rdata + (rb->sz - rb->rd_index), rb->buf, sz - (rb->sz - rb->rd_index));
    } else {
        _memcpy(rdata, rb->buf + rb->rd_index, sz);
    }
    rb->used = rb->used - sz;
    rb->rd_index = (rb->rd_index + sz) % rb->sz;
    return sz;
}

uint32_t rb_used(RingBuffer *rb)
{
    if(!_RB_IS_VALID(rb)) {
        return 0;
    }
    return rb->used;
}

void rb_clr(RingBuffer *rb)
{
    if(!_RB_IS_VALID(rb)) {
        return;
    }
    rb->wr_index = 0;
    rb->rd_index = 0;
    rb->used = 0;
}
