#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "game.h"

#define CLIENT_ID (0)
#define DELAY_MS (10)

static void draw_field(const char field[FIELD_HEIGHT][FIELD_WIDTH]) 
{
    printf("/");
    for (size_t j = 0; j < FIELD_WIDTH; j++) 
    {
        printf("-");
    }
    printf("\\\n");
    for (size_t i = 0; i < FIELD_HEIGHT; i++) 
    {
        printf("|");
        for (size_t j = 0; j < FIELD_WIDTH; j++) 
        {
            printf("%c", field[i][j]);
        }
        printf("|\n");
    }
    printf("\\");
    for (size_t j = 0; j < FIELD_WIDTH; j++) 
    {
        printf("-");
    }
    printf("/");
    printf("\n");
}

int main (void) 
{
    init_game(CLIENT_ID);
    while (1) 
    {
        struct game_state *gs = NULL;

        /* Move current block one column left or right */
        gs = handle_input(CLIENT_ID, (rand() % 2) ? TET_LEFT : TET_RIGHT);
        draw_field((const char (*)[FIELD_WIDTH])gs->field);
        nanosleep(&(struct timespec){0, DELAY_MS*1000*1000}, NULL);

        unsigned int substeps = rand() % (2 * STEP_TIME_INIT/STEP_TIME_GRANULARITY);
        /* Do up to 200% of substeps required for one full time step.
         * Thus for every step right/left above, do up to two steps down. */
        for (size_t i = 0; i < substeps; i++) 
        {
            gs = handle_substep(CLIENT_ID);
        }
        draw_field((const char (*)[FIELD_WIDTH])gs->field);
        if (gs->phase == TET_LOSE || gs->phase == TET_WIN) 
        {
            fprintf(stderr,
                    "Player %s with %u points in level %u.\n",
                    gs->phase == TET_WIN ? "wins" : "loses",
                    gs->points,
                    gs->level);
                exit(1);
        }
        nanosleep(&(struct timespec){0, DELAY_MS*1000*1000}, NULL);
    }
}