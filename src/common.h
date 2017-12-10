#ifndef _COMMON_H_
#define _COMMON_H_

#include <sys/types.h>

#define INVALID_CLIENT_ID (-1)

void bubble_sort(uint32_t list[], long n);
int get_client_id(void);
void release_client_id(uint32_t client_id);
bool client_running(void);
uint32_t time_in_ms(void);
void serialize_data(char data[FIELD_SIZE + 16], struct game_state *gs);

#endif