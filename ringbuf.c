/**
 * @ Author: luoqi
 * @ Create Time: 2024-07-17 11:39
 * @ Modified by: luoqi
 * @ Modified time: 2025-02-26 09:06
 * @ Description:
 */

#include "ringbuf.h"

static inline void *_memcpy(void *dst, const void *src, uint32_t sz)
{
    // Handle overlapping memory regions
    if (dst < src || (char *)dst >= ((char *)src + sz)) {
        // Forward copy for non-overlapping or dst < src
        uint32_t *d32 = (uint32_t *)dst;
        const uint32_t *s32 = (const uint32_t *)src;
        
        // Copy 4 bytes at a time
        while (sz >= 4) {
            *d32++ = *s32++;
            sz -= 4;
        }
        
        // Copy remaining bytes
        uint8_t *d8 = (uint8_t *)d32;
        const uint8_t *s8 = (const uint8_t *)s32;
        while (sz--) {
            *d8++ = *s8++;
        }
    } else {
        // Backward copy for overlapping memory where dst > src
        uint8_t *d = (uint8_t *)dst + sz;
        const uint8_t *s = (const uint8_t *)src + sz;
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
    if(!rb || !data || !sz) {
        return 0;
    }

    if(rb->mutex_lock && rb->mutex_unlock) {
        rb->mutex_lock(); 
    }

    uint32_t total_written = 0;
    uint32_t remaining = sz;

    while (remaining) {
        // Make space if buffer is full
        if (rb->used == rb->sz) {
            rb->rd_index = (rb->rd_index + 1) % rb->sz;
            rb->used--;
        }

        // Calculate maximum contiguous write size
        uint32_t contiguous = rb->sz - rb->wr_index;
        uint32_t to_write = (remaining < contiguous) ? remaining : contiguous;
        to_write = (to_write > (rb->sz - rb->used)) ? (rb->sz - rb->used) : to_write;
        
        // Perform the write
        _memcpy(rb->buf + rb->wr_index, data + total_written, to_write);
        
        // Update indices
        rb->wr_index = (rb->wr_index + to_write) % rb->sz;
        rb->used += to_write;
        total_written += to_write;
        remaining -= to_write;
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
    if(!rb || !rdata) {
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
