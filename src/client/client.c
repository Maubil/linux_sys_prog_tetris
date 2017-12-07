#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <ncurses.h>
#include <signal.h>
#include "../game.h"

#define BUF_SIZE 255

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
            printf("I am connected to host!\n");
            break;
        }

        close(sfd);
    }

    if (rp == NULL)     /* No address succeeded */
    {
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);           /* No longer needed */

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
    char c = 0;
    while (true)
    {
    //TET_CCLOCK,       /* Rotates the current block counterclockwise */
    //TET_CHEAT,        /* Changes the current block to another shape */
    //TET_PAUSE,        /* Pauses/unpauses the current game */
    //TET_FASTER,       /* Increases the pace of the game a bit */
    //TET_SLOWER,*/
        switch(getch())
        {
            case KEY_UP:
                user_input = TET_CLOCK;
                printw("up!");
                break;
            case KEY_DOWN:
                user_input = TET_DOWN;
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
            case 'q':
                printw("q was pressed\n");
                finish(0);
                break;
            case ' ':
                user_input = TET_DOWN_INSTANT;
                printw("space pressed!\n");
                break;
            default:
                user_input = TET_VOID;
                printw("Nothing entered this time...\n");
                break;
        }
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
