#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
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
    enum tet_input user_input = TET_VOID;
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    int ch = 0;
    int row = 0;
    int col = 0;


    WINDOW *my_win;

	refresh();
    getmaxyx(stdscr,row,col);
    printw("row: %d\tcol: %d!", row, col);

	my_win = newwin(FIELD_HEIGHT + 2, FIELD_WIDTH + 2, WIN_POS_X, WIN_POS_Y);
	box(my_win, 0 , 0);
    mvwprintw(my_win, 1, 1,"1234567890");
	wrefresh(my_win);

    while ((ch = getch()) != 'q')
    {
    //TET_CCLOCK,       /* Rotates the current block counterclockwise */
    //TET_CHEAT,        /* Changes the current block to another shape */
    //TET_PAUSE,        /* Pauses/unpauses the current game */
    //TET_FASTER,       /* Increases the pace of the game a bit */
    //TET_SLOWER,*/
        switch(ch)
        {
            case KEY_UP:
                user_input = TET_CLOCK;
                printw("up!");
                mvwprintw(my_win, 1, 1,"0987654321");
                break;
            case KEY_DOWN:
                user_input = TET_DOWN;
                mvwprintw(my_win, 1, 1,"1234567890");
                printw("down!");
                break;
            case KEY_LEFT:
                user_input = TET_LEFT;
                printw("left!");
                break;
            case KEY_RIGHT:
                user_input = TET_RIGHT;
                printw("right!");
                break;
            case 'r':
                user_input = TET_RESTART;
                printw("r was pressed\n");
                break;
            case ' ':
                user_input = TET_DOWN_INSTANT;
                printw("space pressed!\n");
                break;
            default:
                user_input = TET_VOID;
                //printw("Nothing entered this time...\n");
                break;
        }
        wrefresh(my_win);
        mvprintw(row - 1, 0, "Worky worky ?");
        refresh();
/*
        if(send(sock, buffer, strlen(buffer), 0) < 0)
        {
            perror("send()");
            finish(0);
        }

        if((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
        {
            perror("recv()");
            finish(0);
        }
*/
        // DRAW_FIELD!

        napms(100);
    }
    return 0;
}

static void finish(int sig)
{
    endwin();
    /* do your non-curses wrapup here */

    printf("Bye!\n");

    exit(0);
}
