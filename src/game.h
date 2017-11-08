#ifndef GAME_H
#define GAME_H

/* Maximum number of concurrent games */
#define CLIENTS_MAX (5)

/* Various constants used for difficulty, scoring and timing */
#define MAX_LEVEL (5)
#define INIT_LINES_PER_LEVEL (1)
#define TIME_FACTOR_PER_LEVEL (0.7f)
#define STEP_TIME_GRANULARITY (100)
#define STEP_TIME_INIT (1000)

/* Field dimensions */
#define FIELD_WIDTH (10u)
#define FIELD_HEIGHT (18u)
#define FIELD_SIZE (FIELD_WIDTH * FIELD_HEIGHT)

/* The game supports the following input "keys" */
enum tet_input {
    TET_VOID,         /* This key is simply ignored */
    TET_LEFT,         /* Moves the current block one column left */
    TET_RIGHT,        /* Moves the current block one column right */
    TET_DOWN,         /* Moves the current block one row down */
    TET_DOWN_INSTANT, /* Moves the current block down till it collides instantly */
    TET_CLOCK,        /* Rotates the current block clockwise */
    TET_CCLOCK,       /* Rotates the current block counterclockwise */
    TET_CHEAT,        /* Changes the current block to another shape */
    TET_PAUSE,        /* Pauses/unpauses the current game */
    TET_RESTART,      /* Restarts the current game */
    TET_FASTER,       /* Increases the pace of the game a bit */
    TET_SLOWER,       /* Decreases the pace of the game a bit */
};

enum tet_phase {
    TET_LOSE = -1, /* When a game is lost */
    TET_WIN = 0, /* After finishing MAX_LEVEL levels */
    TET_STOPPED = 1, /* While paused */
    TET_IN_PROG = 2, /* While in progress */
};

struct game_state {
    enum tet_phase phase; /* The current phase/state of the game */
    unsigned int points;  /* The current game points a player got */
    unsigned int level;   /* The current level of the game */
    unsigned int togo;    /* The number of lines to clear till next level */
    /* A point to the two-dimensional array representing the play field */
    char (*field)[FIELD_HEIGHT][FIELD_WIDTH];
};

/* Initializes/restarts game i */
void init_game (size_t i);

/* Updates the state of game client_id according to the input in */
struct game_state *handle_input(size_t client_id, enum tet_input in);

/* Handle the timing of the game and needs to be called every STEP_TIME_GRANULARITY milliseconds */
struct game_state *handle_substep(size_t client_id);

#endif // GAME_H