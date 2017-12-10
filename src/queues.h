#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <sys/types.h>

/*! \brief initialize a new queue.
    \return 0 on success, 1 on error.
*/
uint32_t init_queue(void);

/*! \brief cleanup queue.
    \return 0 on success, 1 on error.
*/
uint32_t cleanup_queue(void);

/*! \brief get first element in queue.
    \param retval[out]  point where the element will be stored.
    \return 0 on success, 1 on error.
*/
uint32_t consume(uint32_t *retval);

/*! \brief add element to queue.
    \param value[in]  value to be added to queue.
    \return 0 on success, 1 on error.
*/
uint32_t produce(uint32_t value);

#endif