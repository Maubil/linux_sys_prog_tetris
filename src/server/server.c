#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "../game.h"

#define BUF_SIZE (255)
#define INVALID_CLIENT_ID (-1)
#define DELAY_MS (10)

typedef int SOCKET;

static void print_usage(const char *prog_name);
static int read_server(SOCKET sock, char *buffer);
static void send_data(SOCKET sock, const char *buffer);
static int child_process(SOCKET sock);
static void finish(int sig);
static int get_client_id(void);

int main(int argc, char *argv[])
{
    char c = 0;
    int check_port = 30001;

    (void) signal(SIGINT, finish);

    while ( (c = getopt(argc, argv, "hi:p:")) != -1 ) {
        switch ( c ) {
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

    struct sockaddr_in myaddr ,clientaddr;
    int sockid = socket(AF_INET, SOCK_STREAM, 0);
    memset(&myaddr, '0', sizeof(myaddr));
    myaddr.sin_family=AF_INET;
    myaddr.sin_port=htons(check_port);
    myaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
    if(sockid==-1)
    {
        perror("socket");
    }
    int len=sizeof(myaddr);
    if(bind(sockid,( struct sockaddr*)&myaddr,len)==-1)
    {
        perror("bind");
    }
    if(listen(sockid,10)==-1)
    {
        perror("listen");
    }

    for(;;)
    {
        printf("Accepting connections!\n");
        int new = accept(sockid, (struct sockaddr *)&clientaddr, &len);

        int pid = fork();
        if (pid < 0)
        {
            close(new);
            printf("Fork failed closing socket!\n");
            continue;
        }
        else if(pid == 0)
        {
            /* blocking call */
            int rc = child_process(new);
            (void)rc;
            printf("Child terminated!\n");
            close(new);
            break;
        }
        else
        {
            close(new);
            printf("Parent process!\n");
            continue;
        }
    }

    printf("Exiting for loop now!\n");
    close(sockid);
    return 0;
}

static void print_usage(const char *prog_name)
{
    fprintf(stderr, "Usage: %s [-p <port>] [-h]\n"
                    "Options:\n"
                    "  -p <port>\t\tPort to use.\n"
                    "  -h\t\t\tPrint help and exit.\n",
                    prog_name);
}

static int read_server(SOCKET sock, char *buffer)
{
   int n = 0;

   if((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
   {
      perror("recv()");
      exit(errno);
   }

   buffer[n] = 0;

   return n;
}

static int get_client_id(void)
{
    static bool client_in_use[CLIENTS_MAX] = {0};
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int next_client_id = INVALID_CLIENT_ID;

    pthread_mutex_lock(&mutex);
    for(int i = 0; i < CLIENTS_MAX; i++)
    {
        if(!client_in_use[i])
        {
            next_client_id = i;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);

    return next_client_id;
}
#if 0
static void send_data(SOCKET sock, struct game_state *gs)
{
    void *data = calloc(1, sizeof(struct game_state) + FIELD_SIZE);

struct game_state {
    enum tet_phase phase; /* The current phase/state of the game */
    unsigned int points;  /* The current game points a player got */
    unsigned int level;   /* The current level of the game */
    unsigned int togo;    /* The number of lines to clear till next level */
    /* A point to the two-dimensional array representing the play field */
    char (*field)[FIELD_HEIGHT][FIELD_WIDTH];
};

    data[0] = htonl(gs->phase);
    data[0] = htonl(gs->phase);

    if(send(sock, buffer, strlen(buffer), 0) < 0)
    {
        free(data);
        perror("send()");
        exit(errno);
    }
    free(data);
}
#endif
static int child_process(SOCKET sock)
{
    enum tet_input recvdata = TET_VOID;

    int client_id = get_client_id();
    if (client_id == INVALID_CLIENT_ID)
    {
        // ERROR
        finish(0);
    }

    printf("Child just forked! Starting client %d\n", client_id);

    init_game(client_id);

    while(1)
    {
        struct game_state *gs = NULL;

        //recv(sock, &recvdata, sizeof(struct tet_input), 0);

        /* Move current block one column left or right */
        gs = handle_input(client_id, recvdata);
        nanosleep(&(struct timespec){0, DELAY_MS*1000*1000}, NULL);

        /* Do up to 200% of substeps required for one full time step.
         * Thus for every step right/left above, do up to two steps down. */
        for (size_t i = 0; i < (2 * STEP_TIME_INIT/STEP_TIME_GRANULARITY); i++) 
        {
            gs = handle_substep(client_id);
        }
        if (gs->phase == TET_LOSE || gs->phase == TET_WIN) 
        {
            fprintf(stderr,
                    "Player %s with %u points in level %u.\n",
                    gs->phase == TET_WIN ? "wins" : "loses",
                    gs->points,
                    gs->level);
                exit(1);
        }

        /* serialize data */

        nanosleep(&(struct timespec){0, DELAY_MS*1000*1000}, NULL);
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
