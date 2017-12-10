#ifndef _COMMON_H_
#define _COMMON_H_

#include <sys/types.h>
#include <stdbool.h>

#define INVALID_CLIENT_ID (-1)

/*  \brief bubble sorting, biggest elements will be put first in array.
    \param  list    array to sort.
    \param  n       size of the array.
    \remark from: http://www.programmingsimplified.com/c/source-code/c-program-bubble-sort
*/
void bubble_sort(uint32_t list[], long n);

/*! \brief Get an available client session.
    \return     client id or error
*/
int get_client_id(void);

/*! \brief release a client id.
    \param client_id    id to release.
*/
void release_client_id(uint32_t client_id);

/*! \brief Get the actual time and convert it into milliseconds.
    \return The actual time in ms.
*/
uint32_t time_in_ms(void);

/*! \brief serialize data to send it through a socket.
    \param data[out]    serialized data array.
    \param gs[in]       game structure to serialize.
*/
void serialize_data(char data[FIELD_SIZE + 16], struct game_state *gs);

#endif