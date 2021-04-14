/***************************************************************************//**
 * @file    queue.h
 * @brief   Arduino RTT-Ethernet library queue header
 * @author  onelife <onelife.real[at]gmail.com>
 ******************************************************************************/
#ifndef __QUEUE_H__
#define __QUEUE_H__

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
typedef struct {
    uint32_t front, rear, put_cnt, get_cnt;
    uint32_t capacity;
    void **array;
} queue_t;

/* Exported macros ---------------------------------------------------------- */
#define queue_init(q, b) { \
    q.front = 0; \
    q.rear = sizeof(b) / sizeof(void *) - 1; \
    q.put_cnt = q.get_cnt = 0; \
    q.capacity = q.rear + 1; \
    q.array = b; \
}

/* Exported functions ------------------------------------------------------- */
bool is_full(queue_t *queue);
bool is_empty(queue_t *queue);
bool queue_put(queue_t *queue, void *item);
void *queue_get(queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif
