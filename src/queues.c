#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include "pthread.h"

#define BUFFER_SIZE 50

static pthread_mutex_t lock;
static pthread_cond_t nonempty;
static pthread_cond_t nonfull;
static uint32_t buffer[BUFFER_SIZE];
static uint32_t in; // index of next produced item
static uint32_t out; // index of next item to consume

uint32_t consume(uint32_t *retval) {
  if(pthread_mutex_lock(&lock) != 0)
  {
      return 1;
  }
  // while buffer empty wait for 'nonempty'
  while (in == out) {
    if(pthread_cond_wait(&nonempty, &lock) != 0)
    {
      return 1;
    }
  }
  *retval = buffer[out];
  buffer[out] = 0; // consume element
  out = (out + 1) % BUFFER_SIZE; // update out "pointer"
  if(pthread_cond_signal(&nonfull) != 0)
  {
      return 1;
  }
  if(pthread_mutex_unlock(&lock) != 0)
  {
      return 1;
  }

  return 0;
}

uint32_t produce(uint32_t value) {
  if(pthread_mutex_lock(&lock) != 0)
  {
      return 1;
  }
  // Wait for non-full buffer
  while ((in > out && (in - out) == BUFFER_SIZE - 1) ||
         (in < out && (out - in) == 1)) {
    if(pthread_cond_wait(&nonfull,&lock) != 0)
    {
      return 1;
    }
  }
  buffer[in] = value;
  in = (in + 1) % BUFFER_SIZE; // update in "pointer"
  if(pthread_cond_signal(&nonempty) != 0)
  {
      return 1;
  }
  if(pthread_mutex_unlock(&lock) != 0)
    {
      return 1;
    }
    return 0;
}
// end::funcs[]

uint32_t init_queue(void)
{
    if(pthread_mutex_init(&lock, NULL) != 0)
    {
      return 1;
    }
    if(pthread_cond_init(&nonempty, NULL) != 0)
    {
      return 1;
    }
    if(pthread_cond_init(&nonfull, NULL) != 0)
    {
      return 1;
    }
    return 0;
}

uint32_t cleanup_queue(void)
{
    if(pthread_cond_destroy(&nonempty) != 0)
    {
      return 1;
    }
    if(pthread_cond_destroy(&nonfull) != 0)
    {
      return 1;
    }
    if(pthread_mutex_destroy(&lock) != 0)
    {
      return 1;
    }
    return 0;
}