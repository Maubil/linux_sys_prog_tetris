#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <sys/types.h>

uint32_t consume(uint32_t *retval);
uint32_t produce(uint32_t value);
uint32_t init_queue(void);
uint32_t cleanup_queue(void);

#endif