#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

struct block {
    char *name;
    size_t cols;
    size_t rows;
    char *m;
};

struct block_state {
    struct block cur_block;
    signed int block_rot;
    size_t block_x;
    size_t block_y;
};

struct game_state_int {
    struct game_state gs;
    unsigned int step_time_cur;
    unsigned int step_time_next;
    struct block_state block_state;
    char field[FIELD_HEIGHT][FIELD_WIDTH];
    char canvas[FIELD_HEIGHT][FIELD_WIDTH];
};

static struct game_state_int gstates[CLIENTS_MAX] = {0};

static const struct block blocks[] = {
    {
        .name = "Z",
        .cols = 3,
        .rows = 2,
        .m = (char[]){
            "## "
            " ##"
        },
    }, {
        .name = "L",
        .cols = 3,
        .rows = 2,
        .m = (char[]){
            "###"
            "#  "
        },
    }, {
        .name = "T",
        .cols = 3,
        .rows = 2,
        .m = (char[]){
            "###"
            " # "
        },
    }, {
        .name = "O",
        .cols = 2,
        .rows = 2,
        .m = (char[]){
            "##"
            "##"
        },
    }, {
        .name = "J",
        .cols = 3,
        .rows = 2,
        .m = (char[]){
            "#  "
            "###"
        },
    }, {
        .name = "S",
        .cols = 3,
        .rows = 2,
        .m = (char[]){
            " ##"
            "## "
        },
    }, {
        .name = "I",
        .cols = 1,
        .rows = 4,
        .m = (char[]){
            '#',
            '#',
            '#',
            '#',
        },
    },
};
static const size_t num_blocks = sizeof(blocks)/sizeof(struct block);

static void clear_field(char field[FIELD_HEIGHT][FIELD_WIDTH]) {
    memset(field, ' ', FIELD_SIZE);
}

static bool draw_block(struct game_state_int *gsi, const struct block_state *new_bs, char tmpfield[FIELD_HEIGHT][FIELD_WIDTH]) {
    const struct block *block = &new_bs->cur_block;

    size_t cur_height = new_bs->block_rot % 2 ? block->cols : block->rows;
    /* Avoid falling through the floor */
    if (new_bs->block_y+cur_height > FIELD_HEIGHT) {
        return 1;
    }

    size_t cur_width  = new_bs->block_rot % 2 ? block->rows : block->cols;

    for (size_t cur_row = 0; cur_row < cur_height; cur_row++) {
        for (size_t cur_col = 0; cur_col < cur_width; cur_col++) {
            size_t src_col;
            size_t src_row;
            switch (new_bs->block_rot) {
                case 0:
                    src_col = cur_col;
                    src_row = cur_row;
                    break;
                case 2:
                    src_col = block->cols - 1 - cur_col;
                    src_row = block->rows - 1 - cur_row;
                    break;
                case 3:
                    src_col = block->cols - 1 - cur_row;
                    src_row = cur_col;
                    break;
                case 1:
                    src_col = cur_row;
                    src_row = block->rows - 1 - cur_col;
                    break;
                default:
                    fprintf(stderr, "Unknown block rotation: %d\n", new_bs->block_rot);
                    exit(EXIT_FAILURE);
            }
            char c = *(block->m + src_row * block->cols + src_col);
            /* Ignore "empty" block pixels */
            if (c == ' ')
                continue;

            /* Detect collision with existing blocks */
            if (gsi->field[new_bs->block_y+cur_row][new_bs->block_x+cur_col] != ' ')
                return 1;

            /* Render onto field if requested */
            if (tmpfield != NULL)
                tmpfield[new_bs->block_y+cur_row][new_bs->block_x+cur_col] = c;
        }
    }
    return 0;
}

static void change_step_time (struct game_state_int *gsi, float factor) {
    gsi->step_time_next *= factor;
    if (gsi->step_time_next < STEP_TIME_GRANULARITY)
        gsi->step_time_next = STEP_TIME_GRANULARITY;
    else if (gsi->step_time_next > 2000)
        gsi->step_time_next = 2000;
}

static int new_block(struct game_state_int *gsi) {
    gsi->block_state.cur_block = blocks[rand() % num_blocks];
    /* TODO: more advanced random generator, cf.
     * https://harddrop.com/wiki/Random_Generator
     * https://harddrop.com/wiki/Tetris_(Game_Boy)#Randomizer */
    /* Confine spawns within field widths */
    gsi->block_state.block_x = rand() % (FIELD_WIDTH - gsi->block_state.cur_block.cols);
    gsi->block_state.block_y = 0;
    gsi->block_state.block_rot = 0;
    return draw_block(gsi, &gsi->block_state, gsi->canvas);
}

void init_game (size_t i) {
    gstates[i].step_time_cur = STEP_TIME_INIT;
    gstates[i].step_time_next = STEP_TIME_INIT;
    clear_field(gstates[i].field);
    memcpy(gstates[i].canvas, gstates[i].field, FIELD_SIZE);
    new_block(&gstates[i]);
    gstates[i].gs.phase = TET_IN_PROG;
    gstates[i].gs.points = 0;
    gstates[i].gs.level = 1;
    gstates[i].gs.togo = INIT_LINES_PER_LEVEL;
    gstates[i].gs.field = &gstates[i].field;
}

static void test_remove_lines(struct game_state_int *gsi) {
    ssize_t lines[4] = { -1, -1, -1, -1 };
    size_t idx = 0;
    unsigned int lines_cleared = 0;
    /* Look for full lines and save their index in lines. */
    for (ssize_t i = FIELD_HEIGHT - 1; i >= 0; i--) {
        bool is_line_full = true;
        for (size_t j = 0; j < FIELD_WIDTH; j++) {
            if (gsi->field[i][j] == ' ') {
                is_line_full = false;
                break;
            }
        }
        if (is_line_full) {
            lines_cleared++;
            lines[idx++] = i;
        }
    }

    /* Move field above cleared lines down, starting from the top, and
     * count consecutive lines. */
    unsigned int max_consecutive_lines_cleared = 0;
    for (ssize_t i = idx - 1; i >= 0; i--) {
        if (lines[i] < 0)
            break;

        memmove(gsi->field[1], gsi->field[0], FIELD_WIDTH * lines[i]);
        /* Test if the next found line matches the directly adjacent line (lower field index). */
        if (i < 3 && lines[i+1] == (lines[i]-1))
            max_consecutive_lines_cleared++;
    }
    if (lines_cleared > 0) {
        /* Overwrite lines that have been moved down with spaces. */
        memset(gsi->field[0], ' ', FIELD_WIDTH * lines_cleared);
        max_consecutive_lines_cleared++;
        unsigned int cur_points  = (1 << (max_consecutive_lines_cleared-1)) + lines_cleared;
        gsi->gs.points += cur_points * gsi->gs.level;

        if (gsi->gs.togo <= lines_cleared) {
            /* Level up */
            if (gsi->gs.level >= MAX_LEVEL) {
                gsi->gs.phase = TET_WIN;
                return;
            }
            gsi->gs.level++;
            gsi->gs.togo = gsi->gs.level * INIT_LINES_PER_LEVEL;
            gsi->step_time_next *= TIME_FACTOR_PER_LEVEL;
            fprintf(stderr, "new level %u with interval %u\n", gsi->gs.level, gsi->step_time_next);
        } else {
            gsi->gs.togo -= lines_cleared;
        }
    }
}

static struct game_state *update_state(struct game_state_int *gsi, struct block_state *new_bs, bool down_movement) {
    /* If there is a collision we refrain from accepting the move in general.
     * However, in the case of a down movement the current placement of
     * the block is made permanent by rendering it into the field. */
    if (draw_block(gsi, new_bs, NULL) != 0) {
        if (down_movement) {
            draw_block(gsi, &gsi->block_state, gsi->field);
            test_remove_lines(gsi);
            /* Keep the canvas in sync with the field. */
            memcpy(gsi->canvas, gsi->field, FIELD_SIZE);
            if (new_block(gsi) != 0) {
                gsi->gs.phase = TET_LOSE;
            }
            gsi->gs.field = &gsi->field;
        } else {
            gsi->gs.field = &gsi->canvas;
        }
    } else {
        /* If there is no collision, the new block state is to be
         * committed but the rendered field is only to be sent to the
         * client thus we draw onto the canvas only
         * (which first gets cloned from the field). */
        gsi->block_state = *new_bs;
        memcpy(gsi->canvas, gsi->field, FIELD_SIZE);
        draw_block(gsi, new_bs, gsi->canvas);
        gsi->gs.field = &gsi->canvas;
    }
    return &gsi->gs;
}

static void check_set_rotation(struct block_state *new_bs, int new_rot) {
    /* While rotating a block might get to wide to fit into the field.
     * If that's the case we ignore the respective input.
     * NB: rotating through the floor is catched by the generic check in draw_block(). */
    size_t new_width = new_rot % 2 ? new_bs->cur_block.rows : new_bs->cur_block.cols;
    if (new_bs->block_x + new_width <= FIELD_WIDTH)
        new_bs->block_rot = new_rot;
}

struct game_state *handle_input(size_t client_id, enum tet_input in) {
    struct game_state_int *gsi = &gstates[client_id];
    struct block_state new_bs = gsi->block_state;

    /* Ignore all but pause toggle inputs while paused */
    if (gsi->gs.phase == TET_STOPPED && in != TET_PAUSE) {
        return NULL;
    }

    bool down_movement = false;
    switch (in) {
        case TET_DOWN: {
            new_bs.block_y++;
            down_movement = true;
            break;
        }
        case TET_LEFT: {
            if (new_bs.block_x != 0)
                new_bs.block_x--;
            break;
        }
        case TET_RIGHT: {
            size_t cur_width = new_bs.block_rot % 2 ? new_bs.cur_block.rows : new_bs.cur_block.cols;
            if (new_bs.block_x + cur_width < FIELD_WIDTH)
                new_bs.block_x++;
            break;
        }
        case TET_FASTER: {
            change_step_time(gsi, 0.5f);
            break;
        }
        case TET_SLOWER: {
            change_step_time(gsi, 2.f);
            break;
        }
        case TET_CLOCK: {
            int new_rot = (new_bs.block_rot + 1) % 4;
            check_set_rotation(&new_bs, new_rot);
            break;
        }
        case TET_CCLOCK: {
            int new_rot = new_bs.block_rot == 0 ? 3 : (new_bs.block_rot - 1);
            check_set_rotation(&new_bs, new_rot);
            break;
        }
        case TET_CHEAT: {
            new_bs.cur_block = blocks[rand() % num_blocks];
            break;
        }
        case TET_RESTART: {
            init_game(client_id);
            return NULL;
        }
        case TET_PAUSE: {
            if (gsi->gs.phase == TET_IN_PROG) {
                gsi->gs.phase = TET_STOPPED;
                return NULL;
            } else if (gsi->gs.phase == TET_STOPPED) {
                gsi->gs.phase = TET_IN_PROG;
            }
            break;
        }
        case TET_DOWN_INSTANT: {
            do {
                new_bs.block_y++;
            } while (draw_block(gsi, &new_bs, NULL) == 0);
            gsi->block_state.block_y = new_bs.block_y-1;
            down_movement = true;
            break;
        }
        case TET_VOID: {
            break;
        }
        default: {
            fprintf(stderr, "Unknown input key %d in game %zu\n", in, client_id);
            return NULL;
        }
    }

    return update_state(gsi, &new_bs, down_movement);
}

struct game_state *handle_substep(size_t client_id){
    struct game_state_int *gsi = &gstates[client_id];

    /* Ignore timing while paused */
    if (gsi->gs.phase == TET_STOPPED) {
        return NULL;
    }

    if (gsi->step_time_cur > STEP_TIME_GRANULARITY) {
        gsi->step_time_cur -= STEP_TIME_GRANULARITY;
        gsi->gs.field = &gsi->canvas;
        return &gsi->gs;
    } else {
        gsi->step_time_cur = gsi->step_time_next;

        /* Move down */
        struct block_state new_bs = gsi->block_state;
        new_bs.block_y++;

        return update_state(gsi, &new_bs, true);
    }
}