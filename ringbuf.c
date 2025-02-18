/**
 * @ Author: luoqi
 * @ Create Time: 2024-07-17 11:39
 * @ Modified by: luoqi
 * @ Modified time: 2025-02-18 22:12
 * @ Description:
 */

#include "ringbuf.h"

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

int rb_init(RingBuffer *rb, uint8_t *buf, uint32_t size, int (*mutex_lock)(void), int (*mutex_unlock)(void))
{
    if(!rb || !buf || size == 0) {
        return -1;
    }
    rb->rd_index = 0;
    rb->wr_index = 0;
    rb->used = 0;
    rb->sz = size;
    rb->buf = buf;
    rb->mutex_lock = mutex_lock;
    rb->mutex_unlock = mutex_unlock;
    return 0;
}
uint32_t rb_write_force(RingBuffer *rb, const uint8_t *data, uint32_t sz)
{
    if(!rb && !data) {
        return 0;
    }
    if(rb->mutex_lock && rb->mutex_unlock) {
        rb->mutex_lock(); 
    }
    uint32_t total_written = 0;
    while (sz > 0) {
        if (rb->used == rb->sz) {
            rb->rd_index = (rb->rd_index + 1) % rb->sz;
            rb->used--;
        }
        
        uint32_t free_space = rb->sz - rb->used;
        uint32_t contiguous_free = rb->sz - rb->wr_index;
        uint32_t to_write = (sz < contiguous_free) ? sz : contiguous_free;

        if (to_write > free_space) {
            to_write = free_space;
        }
        
        _memcpy(rb->buf + rb->wr_index, data + total_written, to_write);
        rb->wr_index = (rb->wr_index + to_write) % rb->sz;
        rb->used += to_write;
        total_written += to_write;
        sz -= to_write;
    }
    if(rb->mutex_unlock) {
        rb->mutex_unlock();
    }
    return total_written;
}

uint32_t rb_write(RingBuffer *rb, const uint8_t *data, uint32_t sz)
{
    if(!rb || !data) {
        return 0;
    }
    sz = sz > (rb->sz - rb->used) ? (rb->sz - rb->used) : sz;
    return rb_write_force(rb, data, sz);
}

uint32_t rb_read(RingBuffer *rb, uint8_t *rdata, uint32_t sz)
{
    if(!rb && !rdata) {
        return 0;
    }
    if(rb->mutex_lock && rb->mutex_unlock) {
        rb->mutex_lock();
    }
    if(sz > rb->used) {
        sz = rb->used;
    }
    
    uint32_t total_read = 0;
    while (sz > 0) {
        uint32_t contiguous = rb->sz - rb->rd_index;
        uint32_t to_read = (sz < contiguous) ? sz : contiguous;
        _memcpy(rdata + total_read, rb->buf + rb->rd_index, to_read);
        rb->rd_index = (rb->rd_index + to_read) % rb->sz;
        rb->used -= to_read;
        total_read += to_read;
        sz -= to_read;
    }
    if(rb->mutex_unlock) {
        rb->mutex_unlock();
    }
    return total_read;
}

uint32_t rb_used(RingBuffer *rb)
{
    if(!rb) {
        return 0;
    }
    if(rb->mutex_lock && rb->mutex_unlock) {
        rb->mutex_lock();
    }
    uint32_t used = rb->used;
    if(rb->mutex_unlock) {
        rb->mutex_unlock();
    }
    return used;
}

void rb_clr(RingBuffer *rb)
{
    if(!rb) {
        return;
    }
    if(rb->mutex_lock && rb->mutex_unlock) {
        rb->mutex_lock();
    }
    rb->wr_index = 0;
    rb->rd_index = 0;
    rb->used = 0;
    if(rb->mutex_unlock) {
        rb->mutex_unlock();
    }
}
