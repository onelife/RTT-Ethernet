/***************************************************************************//**
 * @file    queue.c
 * @brief   Arduino RTT-Ethernet library queue function
 * @author  onelife <onelife.real[at]gmail.com>
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "queue.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/* Public functions ----------------------------------------------------------*/
/*******************************************************************************
                       Queue implemented by array 
REF: https://www.geeksforgeeks.org/queue-set-1introduction-and-array-implementation/
*******************************************************************************/
bool is_full(queue_t *queue) {
  if (queue->put_cnt < queue->get_cnt)
    return (((uint32_t)-1 - queue->get_cnt + queue->put_cnt) >= queue->capacity);
  return ((queue->put_cnt - queue->get_cnt) >= queue->capacity);
}

bool is_empty(queue_t *queue) {
  return (queue->put_cnt == queue->get_cnt);
}

bool queue_put(queue_t *queue, void *item) {
  if (is_full(queue))
    return false;
  queue->rear = (queue->rear + 1) % queue->capacity;
  queue->array[queue->rear] = item;
  queue->put_cnt++;
  return true;
}

void *queue_get(queue_t *queue) {
  void *item;
  if (is_empty(queue))
    return NULL;
  item = queue->array[queue->front];
  queue->front = (queue->front + 1) % queue->capacity;
  queue->get_cnt++;
  return item;
}
