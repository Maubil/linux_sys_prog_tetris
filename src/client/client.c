#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <sys/socket.h>
#include <ncurses.h>
#include <signal.h>
#include "../game.h"

#define BUF_SIZE 255
#define WIN_POS_X 10
#define WIN_POS_Y 1

static void print_usage(const char *prog_name);
static int init_connection(const char *server_ip, const char *server_port);
static int game_session(int sock);
static void finish(int sig);
static void recv_data(int sock, struct game_state *gs);
WINDOW *field_draw(const char field[FIELD_HEIGHT][FIELD_WIDTH]);

int main(int argc, char *argv[])
{
    char c = 0;
    char *server_ip = "127.0.0.1";
    char *server_port = "30001";
    int check_port = 0;
    int sock = 0;

    (void) signal(SIGINT, finish);

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

    int rc = game_session(sock);

    close(sock);
    finish(0);
    return 0;
}

/*! \brief Initialize the connection to the remote server.
    \param server_ip    server IP string formatted.
    \param server_port  server port string formatted.
    \return socket number
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
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
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
            printf("Connected to server!\n");
            break;
        }

        close(sfd);
    }

    freeaddrinfo(result);

    if (rp == NULL)     /* No address succeeded */
    {
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    return sfd;
}

static void print_usage(const char *prog_name)
{
    fprintf(stderr, "Usage: %s [-i <server ip>] [-p <server port>] [-h]\n"
                    "Options:\n"
                    "  -i <server ip>\t\tIP adresse of the server.\n"
                    "  -p <server port>\t\tServer port in use.\n"
                    "  -h\t\t\t\tPrint help and exit.\n",
                    prog_name);
}

static int game_session(int sock)
{
    char field[FIELD_HEIGHT][FIELD_WIDTH];
    struct game_state gs = {0};
    WINDOW *my_win = NULL;
    int ch = 0;
    int row = 0;
    int col = 0;
    
    memset(field, ' ', FIELD_SIZE);
    gs.field = field;

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
	refresh();
    getmaxyx(stdscr, row, col);
    printw("row: %d\tcol: %d!", row, col);

	my_win = field_draw(&field[0]);
    char user_input = TET_VOID;

    while ((ch = getch()) != 'q')
    {
        mvprintw(row - 1, 0, "Worky worky ?");
        refresh();

        recv_data(sock, &gs);

           //TET_CCLOCK,       /* Rotates the current block counterclockwise */
    //TET_CHEAT,        /* Changes the current block to another shape */
    //TET_PAUSE,        /* Pauses/unpauses the current game */
    //TET_FASTER,       /* Increases the pace of the game a bit */
    //TET_SLOWER,*/
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
            default:
                user_input = TET_VOID;
                break;
        }

        if(send(sock, &user_input, 1, 0) < 0)
        {
            finish(2);
            perror("send()");
            exit(2);
        }

        delwin(my_win);
        my_win = field_draw(&gs.field[0]);

        napms(100);
    }
    return 0;
}

static void recv_data(int sock, struct game_state *gs)
{
    unsigned char data[FIELD_SIZE + 16] = {0};
    char *ptr = &gs->field[0];
    ssize_t n = 0; // TODO check n

    if((n = recv(sock, data, FIELD_SIZE + 16, 0)) < 0)
    {
        perror("recv()");
        finish(3);
    }

    gs->phase = data[0] || data[1] << 8 ||  data[2] << 16 || data[3] << 24;
    gs->points = data[4] || data[5] << 8 ||  data[6] << 16 || data[7] << 24;
    gs->level = data[8] || data[9] << 8 ||  data[10] << 16 || data[11] << 24;
    gs->togo = data[12] || data[13] << 8 ||  data[14] << 16 || data[15] << 24;

    for(int i = 0; i < FIELD_SIZE; i++)
    {
        *ptr = data[i + 16];
        ptr++;
    }
}

static void finish(int sig)
{
    endwin();
    /* do your non-curses wrapup here */

    printf("Bye!");

    //exit(sig);
}

WINDOW *field_draw(const char field[FIELD_HEIGHT][FIELD_WIDTH])
{
    WINDOW *local_win = newwin(FIELD_HEIGHT + 2, FIELD_WIDTH + 2, WIN_POS_X, WIN_POS_Y);
	box(local_win, 0 , 0);
    for(int i = 0; i < FIELD_HEIGHT; i++)
    {
        for(int j = 0; j < FIELD_WIDTH; j++)
        {
            mvwaddch(local_win, i + 1, j + 1, field[i][j]);
        }
    }
	wrefresh(local_win);

    return local_win;
}
