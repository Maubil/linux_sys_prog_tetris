#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <ncurses.h>
#include <signal.h>
#include "game.h"
#include "common.h"

#define BUF_SIZE 255
#define WIN_POS_X 2
#define WIN_POS_Y 1
#define MIN_LINES ((int)(FIELD_HEIGHT + 7u))
#define MIN_COLS  ((int)(FIELD_WIDTH + 25))
#define NB_HIGH_SCORES_SHOWN (10)
#define SERVER_DEFAULT_PORT "30001"
#define SERVER_DEFAULT_IP   "127.0.0.1"
#define CLEAR_SCREEN_TIME   3000

WINDOW *my_win = NULL;
struct game_state gs = {0};
int sock = 0;

static void print_usage(const char *prog_name);
static int init_connection(const char *server_ip, const char *server_port);
static int game_session(void);
static void recv_data(struct game_state *gs);
static void show_high_scores(void);
static void finish(int sig);
WINDOW *field_draw(const char field[FIELD_HEIGHT][FIELD_WIDTH]);

int main(int argc, char *argv[])
{
    char c = 0;
    char *server_ip = SERVER_DEFAULT_IP;
    char *server_port = SERVER_DEFAULT_PORT;
    int32_t check_port = 0;

    if (signal(SIGINT, finish) == SIG_ERR) {
        perror(0);
        exit(1);
    }

    while ( (c = getopt(argc, argv, "hi:p:")) != -1 ) {
        switch ( c ) {
            case 'i':
                /* user passed server IP */
                server_ip = optarg;
                break;

            case 'p':
                /* user passed server port */
                check_port = atoi(optarg);
                /* only allow user ranged ports and not terinet port */
                if(check_port >= 65535 || check_port <= 1024 || check_port == 31457)
                {
                    print_usage(argv[0]);
                    return 1;
                }
                server_port = optarg;
                break;

            case 'h':
                /* print usage and exit without failure */
                print_usage(argv[0]);
                return 0;

            case '?':
                /* wrong usagem print usage and return with an error */
                print_usage(argv[0]);
                return 1;
        }
    }

    /* we are ready to start the game */
    sock = init_connection(server_ip, server_port);

    int rc = game_session();

    finish(0);
    return rc;
}

/*! \brief First communication with server, will fetch and show high scores.
*/
static void show_high_scores(void)
{
    char high_score[NB_HIGH_SCORES_SHOWN * 4] = {0};

    if(recv(sock, high_score, sizeof(high_score) / sizeof(high_score[0]), 0) < 0)
    {
        perror("recv()");
        exit(EXIT_FAILURE);
    }

    if(mvprintw(0, 0, "High scores. Beat them ;) !") == ERR)
    {
        perror("mvprintw()");
        exit(EXIT_FAILURE);
    }

    for(size_t i = 0; i < NB_HIGH_SCORES_SHOWN; i++)
    {
        if(mvprintw(i + 2, 0, "%02d. with a score of %u", i + 1, high_score[i * 4] | high_score[i * 4 + 1] | high_score[i * 4 + 2] | high_score[i * 4 + 3]) == ERR)
        {
            perror("mvprintw()");
            exit(EXIT_FAILURE);
        }
    }
    if(mvprintw(NB_HIGH_SCORES_SHOWN + 3, 0, "Press any key to start the game!") == ERR)
    {
        perror("mvprintw()");
        exit(EXIT_FAILURE);
    }

    if(refresh() == ERR)
    {
        perror("refresh()");
        exit(EXIT_FAILURE);
    }

    /* blocking call to wait on user input before starting the game */
    (void)getchar();

    char data = (char)TET_VOID;
    if(send(sock, &data, 1, 0) < 0)
    {
        perror("send()");
        exit(EXIT_FAILURE);
    }
}

/*! \brief Initialize the connection to the remote server.
    \param server_ip    server IP string formatted.
    \param server_port  server port string formatted.
    \other  inspired by the getaddrinfo manual example.
*/
static int init_connection(const char *server_ip, const char *server_port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s;
    int sfd;

    /* Obtain address(es) matching host/port */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* stream socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    s = getaddrinfo(server_ip, server_port, &hints, &result);
    if (s != 0) {
        (void)fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully connect(2).
        If socket(2) (or connect(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
        {
            continue;
        }

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
        {
            /* success */
            (void)printf("Connected to server!\n");
            break;
        }

        close(sfd);
    }

    freeaddrinfo(result);

    if (rp == NULL)     /* No address succeeded */
    {
        (void)fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    return sfd;
}

/*! \brief print usage to sterr
    \param prog_name    program name string
*/
static void print_usage(const char *prog_name)
{
    (void)fprintf(stderr, "Usage: %s [-i <server ip>] [-p <server port>] [-h]\n"
                    "Options:\n"
                    "  -i <server ip>\t\tIP adresse of the server.\n"
                    "  -p <server port>\t\tServer port in use.\n"
                    "  -h\t\t\t\tPrint help and exit.\n",
                    prog_name);
}

/*! \brief Start a game session.
    \return 0 on success 1 on error
*/
static int game_session(void)
{
    char field[FIELD_HEIGHT][FIELD_WIDTH];
    uint32_t last_handling = time_in_ms();
    int ch = 0;
    
    memset(field, ' ', FIELD_SIZE);
    gs.field = &field;

    initscr();
    if(cbreak() == ERR)
    {
        perror("cbreak()");
        exit(EXIT_FAILURE);
    }
    if(noecho() == ERR)
    {
        perror("noecho()");
        exit(EXIT_FAILURE);
    }
    if(curs_set(0) == ERR)
    {
        perror("curs_set()");
        exit(EXIT_FAILURE);
    }
    if(keypad(stdscr, TRUE) == ERR)
    {
        perror("keypad()");
        exit(EXIT_FAILURE);
    }
    if(nodelay(stdscr, TRUE) == ERR)
    {
        perror("nodelay()");
        exit(EXIT_FAILURE);
    }
	if(refresh() == ERR)
    {
        perror("refresh()");
        exit(EXIT_FAILURE);
    }
    

    show_high_scores();
    if(clear() == ERR)
    {
        perror("clear()");
        exit(EXIT_FAILURE);
    }

	my_win = field_draw((const char (*)[FIELD_WIDTH])gs.field);
    char user_input = TET_VOID;

    while ((ch = getch()) != 'q')
    {
        recv_data(&gs);

        if(gs.phase == TET_LOSE)
        {
            break;
        }

        switch(ch)
        {
            case KEY_UP:
                user_input = TET_CLOCK;
                break;
            case KEY_DOWN:
                user_input = TET_DOWN;
                break;
            case KEY_LEFT:
                user_input = TET_LEFT;
                break;
            case KEY_RIGHT:
                user_input = TET_RIGHT;
                break;
            case 'r':
                user_input = TET_RESTART;
                break;
            case ' ':
                user_input = TET_DOWN_INSTANT;
                break;
            case 'm':
                user_input = TET_CCLOCK;
                break;
            case 'c':
                user_input = TET_CHEAT;
                break;
            case 'p':
                user_input = TET_PAUSE;
                break;
            case 'f':
                user_input = TET_FASTER;
                break;
            case 's':
                user_input = TET_SLOWER;
                break;
            default:
                user_input = TET_VOID;
                break;
        }

        if(send(sock, &user_input, 1, 0) < 0)
        {
            perror("send()");
            exit(EXIT_FAILURE);
        }

        delwin(my_win);

        /* every n seconds, clear the screen completely to remove possible residual text messages (due to reisizes...) */
        if((time_in_ms() - last_handling) > CLEAR_SCREEN_TIME)
        {
            clear();
            last_handling = time_in_ms();
        }
        if(mvprintw(0, 0, "lines: %d\tcol: %d!", LINES, COLS) == ERR)
        {
            perror("printw()");
            break;
        }
        if(mvprintw(LINES - 2, 0, "Level %d, score is %d, %d lines\n are needed until next level!", gs.level, gs.points, gs.togo) == ERR)
        {
            perror("mvprintw()");
            break;
        }
        refresh();
        my_win = field_draw((const char (*)[FIELD_WIDTH])gs.field);

        napms(50);
    }

    return 0;
}

/*! \brief receive data and deserialize it.
    \param gs   game status pointer.
*/
static void recv_data(struct game_state *gs)
{
    char data[FIELD_SIZE + 16] = {0};
    char *ptr = &(*gs->field)[0][0];
    ssize_t n = 0;

    if((n = recv(sock, data, sizeof(data) / sizeof(data[0]), 0)) < 0)
    {
        perror("recv()");
        exit(EXIT_FAILURE);
    }

    gs->phase  = (enum tet_phase)data[0];
    gs->points = data[4] | data[5] << 8 | data[6] << 16 | data[7] << 24;
    gs->level  = data[8] | data[9] << 8 | data[10] << 16 | data[11] << 24;
    gs->togo   = data[12] | data[13] << 8 | data[14] << 16 | data[15] << 24;

    for(size_t i = 0; i <= FIELD_SIZE; i++)
    {
        *ptr = data[i + 16];
        ptr++;
    }
}

/*! \brief Draw a window with the tetris field.
    \param  field    actual field status
    \return WINDOW pointer
*/
WINDOW *field_draw(const char field[FIELD_HEIGHT][FIELD_WIDTH])
{
    WINDOW *local_win = newwin(FIELD_HEIGHT + 2, FIELD_WIDTH + 2, WIN_POS_X, WIN_POS_Y);
	box(local_win, 0, 0);
    for(size_t i = 0; i < FIELD_HEIGHT; i++)
    {
        for(size_t j = 0; j < FIELD_WIDTH; j++)
        {
            if(mvwaddch(local_win, i + 1, j + 1, field[i][j]) == ERR)
            {
                perror("mvwaddch()");
                exit(EXIT_FAILURE);
            }
        }
    }
	if(wrefresh(local_win) == ERR)
    {
        perror("wrefresh()");
        exit(EXIT_FAILURE);
    }

    return local_win;
}

/*! \brief  restore terminal 
    \param  sig signal which has been triggered
*/
static void finish(int sig)
{
    const char user_input = 'q';
    if(send(sock, &user_input, 1, 0) < 0)
    {
        perror("send()");
        exit(EXIT_FAILURE);
    }

    close(sock);

    delwin(my_win);
    endwin();

    (void)printf("You %s with %u points in level %u.\n", gs.phase == TET_WIN ? "won" : "lose", gs.points, gs.level);

    exit(0);
}