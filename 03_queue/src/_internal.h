#pragma once

#include "../include/queue.h"

/**
 * @brief   실제 큐 구조체
 */
struct queue_internal {
    struct node* front; /**< 큐의 front node */
    struct node* rear;  /**< 큐의 rear node */

    int size;           /**< 큐의 요소 개수 */
};