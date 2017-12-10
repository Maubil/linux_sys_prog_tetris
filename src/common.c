#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdbool.h>
#include "game.h"
#include "common.h"

static uint8_t clients_in_use[CLIENTS_MAX] = {0};
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void bubble_sort(uint32_t list[], size_t n)
{
    uint32_t c, d, t;

    for (c = 0 ; c < ( n - 1 ); c++)
    {
        for (d = 0 ; d < n - c - 1; d++)
        {
            if (list[d] < list[d+1])
            {
            /* Swapping */
            t         = list[d];
            list[d]   = list[d+1];
            list[d+1] = t;
            }
        }
    }
}

int get_client_id(void)
{
    int next_client_id = INVALID_CLIENT_ID;

    if(pthread_mutex_lock(&mutex) != 0)
    {
        perror("pthread_mutex_lock()");
        exit(1);
    }
    for(size_t i = 0; i < CLIENTS_MAX; i++)
    {
        if(clients_in_use[i] == 0)
        {
            next_client_id = i;
            clients_in_use[i] = 1;
            break;
        }
    }
    if(pthread_mutex_unlock(&mutex) != 0)
    {
        perror("pthread_mutex_unlock()");
        exit(1);
    }

    return next_client_id;
}

void release_client_id(uint32_t client_id)
{
    if(client_id >= CLIENTS_MAX)
    {
        return;
    }
    if(pthread_mutex_lock(&mutex) != 0)
    {
        perror("pthread_mutex_lock()");
        exit(1);
    }
    clients_in_use[client_id] = 0;
    if(pthread_mutex_unlock(&mutex) != 0)
    {
        perror("pthread_mutex_unlock()");
        exit(1);
    }
}

uint32_t time_in_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (uint32_t)(((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

void serialize_data(char data[FIELD_SIZE + 16], struct game_state *gs)
{
    char *ptr = &(*gs->field)[0][0];

    data[0] = (char)gs->phase;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;

    data[4] = (char)gs->points;
    data[5] = (char)(gs->points >> 8);
    data[6] = (char)(gs->points >> 16);
    data[7] = (char)(gs->points >> 24);

    data[8] = (char)gs->level;
    data[9] = (char)(gs->level >> 8);
    data[10] = (char)(gs->level >> 16);
    data[11] = (char)(gs->level >> 24);

    data[12] = (char)gs->togo;
    data[13] = (char)(gs->togo >> 8);
    data[14] = (char)(gs->togo >> 16);
    data[15] = (char)(gs->togo >> 24);

    for(size_t i = 0; i <= FIELD_SIZE; i++)
    {
        data[i + 16] = *ptr;
        ptr++;
    }
}