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

#define INVALID_CLIENT_ID (-1)
#define DELAY_MS (200)

static void print_usage(const char *prog_name);
static void send_data(int sock, struct game_state *gs);
static int child_process(int sock);
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
    socklen_t my_addr_len=sizeof(myaddr);
    if(bind(sockid,( struct sockaddr*)&myaddr, my_addr_len)==-1)
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
        socklen_t client_addr_len=sizeof(clientaddr);
        int new = accept(sockid, (struct sockaddr *)&clientaddr, &client_addr_len);

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
            client_in_use[i] = 1;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);

    return next_client_id;
}

static int send_data(int sock, struct game_state *gs)
{
    unsigned char data[FIELD_SIZE + 16] = {0};
    char *ptr = &gs->field[0];
    int rc = 0;

    data[0] = gs->phase & 0x000000ff;
    data[1] = gs->phase & 0x0000ff00;
    data[2] = gs->phase & 0x00ff0000;
    data[3] = gs->phase & 0xff000000;

    data[4] = gs->points & 0x000000ff;
    data[5] = gs->points & 0x0000ff00;
    data[6] = gs->points & 0x00ff0000;
    data[7] = gs->points & 0xff000000;

    data[8] = gs->level & 0x000000ff;
    data[9] = gs->level & 0x0000ff00;
    data[10] = gs->level & 0x00ff0000;
    data[11] = gs->level & 0xff000000;

    data[12] = gs->togo & 0x000000ff;
    data[13] = gs->togo & 0x0000ff00;
    data[14] = gs->togo & 0x00ff0000;
    data[15] = gs->togo & 0xff000000;

    for(int i = 0; i < FIELD_SIZE; i++)
    {
        data[i + 16] = *ptr;
        ptr++;
    }
 
    if(send(sock, data, FIELD_SIZE + 16, 0) < 0)
    {
        perror("send()");
        rc = 1;
    }
    return rc;
}

static int child_process(int sock)
{
    enum tet_input user_input = TET_VOID;
    char recv_data = 0;

    int client_id = get_client_id();
    if (client_id == INVALID_CLIENT_ID)
    {
        // ERROR
        finish(1);
    }

    printf("Child just forked! Starting client %d\n", client_id);

    init_game(client_id);

    while(1)
    {
        struct game_state *gs = NULL;

        /* Move current block one column left or right */
        gs = handle_input(client_id, recv_data);
        if(gs == NULL)
        {
            // error
            break;
        }
        if(nanosleep(&(struct timespec){0, DELAY_MS*1000*1000}, NULL) != 0)
        {
            // error
            perror("nanosleep()");
            break;
        }

        /* Do up to 200% of substeps required for one full time step.
         * Thus for every step right/left above, do up to two steps down. */
        for (size_t i = 0; i < (2 * STEP_TIME_INIT/STEP_TIME_GRANULARITY); i++) 
        {
            gs = handle_substep(client_id);
        }
        if (gs->phase == TET_LOSE || gs->phase == TET_WIN) 
        {
            fprintf(stderr, "Player %s with %u points in level %u.\n",
                    gs->phase == TET_WIN ? "wins" : "loses", gs->points, gs->level);
            break;
        }
        if(send_data(sock, gs) != 0)
        {
            // error
            break;
        }
        if(recv(sock, &recv_data, 1, 0) < 0)
        {
            // error
            perror("recv()");
            break;
        }

        if(nanosleep(&(struct timespec){0, DELAY_MS*1000*1000}, NULL) != 0)
        {
            // error
            perror("nanosleep()");
            break;
        }
    }
    
    return 0;
}

static void finish(int sig)
{
    printf("Bye!\n");

    exit(sig);
}
