/**
 * @ Author: luoqi
 * @ Create Time: 2024-07-17 11:39
 * @ Modified by: luoqi
 * @ Modified time: 2025-07-20 14:36
 * @ Description: Header file for the ring buffer library
 */

#ifndef _RINGBUF_H
#define _RINGBUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#ifndef QNULL
#define QNULL ((void *)0)
#endif

/**
 * @brief Structure representing a ring buffer.
 */
typedef struct {
    size_t wr_index;    // Write index of the ring buffer
    size_t rd_index;    // Read index of the ring buffer
    size_t used;        // Used size of the ring buffer
    size_t sz;          // Total size of the ring buffer
    uint8_t  *buf;      // Pointer to the buffer
    void (*lock)(void);  // Pointer to the mutex lock function
    void (*unlock)(void); // Pointer to the mutex unlock function
} RingBuffer;

/**
 * @brief Initialize the ring buffer.
 * @param rb Pointer to the ring buffer structure.
 * @param buf Pointer to the buffer.
 * @param sz Size of the buffer.
 * @param mutex_lock Pointer to the mutex lock function.
 * @param mutex_unlock Pointer to the mutex unlock function.
 * @return 0 on success, -1 on failure.
 */
int rb_init(RingBuffer *rb, uint8_t *buf, size_t sz, void (*lock)(void), void (*unlock)(void));

/**
 * @brief Write data to the ring buffer. Write only part of the data if there is insufficient space.
 * @param rb Pointer to the ring buffer structure.
 * @param data Pointer to the data to be written.
 * @param sz Size of the data to be written.
 * @return The actual number of bytes written.
 */
size_t rb_write(RingBuffer *rb, const void *data, size_t sz);

/**
 * @brief Force write data to the ring buffer. Overwrite old data if there is insufficient space.
 * @param rb Pointer to the ring buffer structure.
 * @param data Pointer to the data to be written.
 * @param sz Size of the data to be written.
 * @return The actual number of bytes written.
 */
size_t rb_write_force(RingBuffer *rb, const void *data, size_t sz);

/**
 * @brief Read data from the ring buffer.
 * @param rb Pointer to the ring buffer structure.
 * @param rdata Pointer to store the read data.
 * @param sz Expected number of bytes to read.
 * @return The actual number of bytes read.
 */
size_t rb_read(RingBuffer *rb, void *rdata, size_t sz);

/**
 * @brief Get the used size of the ring buffer.
 * @param rb Pointer to the ring buffer structure.
 * @return The used size of the ring buffer.
 */
size_t rb_used(RingBuffer *rb);

/**
 * @brief Clear the ring buffer.
 * @param rb Pointer to the ring buffer structure.
 */
void rb_clr(RingBuffer *rb);

#ifdef __cplusplus
}
#endif
#endif
