/**
 * @ Author: luoqi
 * @ Create Time: 2025-06-24 21:06
 * @ Modified by: luoqi
 * @ Modified time: 2025-07-20 14:37
 * @ Description:
 */

#include "ringbuf.h"
#include <stdint.h>

#define RB_IS_ALIGNED(p, n) (((uintptr_t)(p) & ((n) - 1)) == 0)

static void *_memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = dest;
    const uint8_t *s = src;

    if(d > s && d < s + n) {
        d += n - 1;
        s += n - 1;
        while(n--) {
            *d-- = *s--;
        }
    } else {
        if(n >= 8 && RB_IS_ALIGNED(s, 8) && RB_IS_ALIGNED(d, 8)) {
            uint64_t *d64 = (uint64_t *)d;
            const uint64_t *s64 = (const uint64_t *)s;
            while(n >= 8) {
                *d64++ = *s64++;
                n -= 8;
            }
            d = (uint8_t *)d64;
            s = (uint8_t *)s64;
        } else if(n >= 4 && RB_IS_ALIGNED(s, 4) && RB_IS_ALIGNED(d, 4)) {
            uint32_t *d32 = (uint32_t *)d;
            const uint32_t *s32 = (const uint32_t *)s;
            while(n >= 4) {
                *d32++ = *s32++;
                n -= 4;
            }
            d = (uint8_t *)d32;
            s = (uint8_t *)s32;
        }

        while(n--) {
            *d++ = *s++;
        }
    }

    return dest;
}

int rb_init(RingBuffer *rb, uint8_t *buf, size_t sz, void (*lock)(void), void (*unlock)(void))
{
    if(!rb || !buf || sz == 0) {
        return -1;
    }
    rb->rd_index = 0;
    rb->wr_index = 0;
    rb->used = 0;
    rb->sz = sz;
    rb->buf = buf;
    rb->lock = lock;
    rb->unlock = unlock;
    return 0;
}

size_t rb_write_force(RingBuffer *rb, const void *data, size_t sz)
{
    if(!rb || !data || !sz) {
        return 0;
    }
    const uint8_t *p = (const uint8_t *)data;
    if(rb->lock && rb->unlock) {
        rb->lock();
    }

    size_t total_written = 0;
    size_t remaining = sz;

    while(remaining) {
        if(remaining > rb->sz - rb->used) {
            size_t need_space = remaining - (rb->sz - rb->used);
            size_t move_size = (need_space < rb->used) ? need_space : rb->used;
            rb->rd_index = (rb->rd_index + move_size) % rb->sz;
            rb->used -= move_size;
        }

        size_t contiguous = rb->sz - rb->wr_index;
        size_t to_write = (remaining < contiguous) ? remaining : contiguous;
        to_write = (to_write > (rb->sz - rb->used)) ? (rb->sz - rb->used) : to_write;

        _memcpy(rb->buf + rb->wr_index, p + total_written, to_write);

        rb->wr_index = (rb->wr_index + to_write) % rb->sz;
        rb->used += to_write;
        total_written += to_write;
        remaining -= to_write;
    }

    if(rb->unlock) {
        rb->unlock();
    }
    return total_written;
}

size_t rb_write(RingBuffer *rb, const void *data, size_t sz)
{
    if(!rb || !data) {
        return 0;
    }
    sz = sz > (rb->sz - rb->used) ? (rb->sz - rb->used) : sz;
    return rb_write_force(rb, data, sz);
}

size_t rb_read(RingBuffer *rb, void *rdata, size_t sz)
{
    if(!rb || !rdata) {
        return 0;
    }
    uint8_t *p = (uint8_t *)rdata;
    if(rb->lock && rb->unlock) {
        rb->lock();
    }
    if(sz > rb->used) {
        sz = rb->used;
    }

    size_t total_read = 0;
    while(sz > 0) {
        size_t contiguous = rb->sz - rb->rd_index;
        size_t to_read = (sz < contiguous) ? sz : contiguous;
        _memcpy(p + total_read, rb->buf + rb->rd_index, to_read);
        rb->rd_index = (rb->rd_index + to_read) % rb->sz;
        rb->used -= to_read;
        total_read += to_read;
        sz -= to_read;
    }
    if(rb->unlock) {
        rb->unlock();
    }
    return total_read;
}

size_t rb_used(RingBuffer *rb)
{
    if(!rb) {
        return 0;
    }
    if(rb->lock && rb->unlock) {
        rb->lock();
    }
    size_t used = rb->used;
    if(rb->unlock) {
        rb->unlock();
    }
    return used;
}

void rb_clr(RingBuffer *rb)
{
    if(!rb) {
        return;
    }
    if(rb->lock && rb->unlock) {
        rb->lock();
    }
    rb->wr_index = 0;
    rb->rd_index = 0;
    rb->used = 0;
    if(rb->unlock) {
        rb->unlock();
    }
}
