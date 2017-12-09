#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include "../game.h"

#define INVALID_CLIENT_ID (-1)
#define DELAY_MS (200)

static int clients_in_use = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void print_usage(const char *prog_name);
static int send_data(int sock, struct game_state *gs);
static int child_process(int sock, int client_id);
static void finish(int sig);
static int get_client_id(void);

int main(int argc, char *argv[])
{
    char c = 0;
    int sockid = 0;
    int check_port = 30001;
    struct sockaddr_in myaddr, clientaddr;

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

    sockid = socket(AF_INET, SOCK_STREAM, 0);
    if(sockid==-1)
    {
        perror("socket");
        return 1;
    }
    
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family=AF_INET;
    myaddr.sin_port=htons(check_port);
    myaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
    socklen_t my_addr_len=sizeof(myaddr);
    if(bind(sockid, (struct sockaddr*)&myaddr, my_addr_len) == -1)
    {
        perror("bind");
        return 1;
    }
    if(listen(sockid, 10) == -1)
    {
        perror("listen");
        return 1;
    }

    while(1)
    {
        (void)printf("Accepting connections!\n");
        socklen_t client_addr_len=sizeof(clientaddr);
        int new = accept(sockid, (struct sockaddr *)&clientaddr, &client_addr_len);

        int pid = fork();
        if (pid < 0)
        {
            close(new);
            (void)printf("Fork failed closing socket!\n");
            continue;
        }
        else if(pid == 0)
        {
            /* blocking call */
            printf("New child started!\n");
            int client_id = get_client_id();
            if (client_id != INVALID_CLIENT_ID)
            {
                child_process(new, client_id);
            }
            else
            {
                (void)printf("no available session");
            }
            (void)printf("Child finished!\n");
            close(new);
            break;
        }
        else
        {
            close(new);
            (void)printf("Parent process!\n");
            continue;
        }
    }

    if(pthread_mutex_lock(&mutex) != 0)
    {
        exit(1);
    }
    if(clients_in_use == 0)
    {
        (void)printf("No more childrens running!\n");
        close(sockid);
    }
    if(pthread_mutex_unlock(&mutex) != 0)
    {
        exit(1);
    }
    return 0;
}

/*! \brief print usage to sterr
    \param prog_name    program name string
*/
static void print_usage(const char *prog_name)
{
    (void)fprintf(stderr, "Usage: %s [-p <port>] [-h]\n"
                    "Options:\n"
                    "  -p <port>\t\tPort to use.\n"
                    "  -h\t\t\tPrint help and exit.\n",
                    prog_name);
}

/*! \brief Get an available client session.
    \return     client id or error
*/
static int get_client_id(void)
{
    int next_client_id = INVALID_CLIENT_ID;

    if(pthread_mutex_lock(&mutex) != 0)
    {
        exit(1);
    }
    if(clients_in_use < CLIENTS_MAX)
    {
        next_client_id = clients_in_use++;
    }
    if(pthread_mutex_unlock(&mutex) != 0)
    {
        exit(1);
    }

    return next_client_id;
}

/*! \brief serialize and send game data to client.
    \param sock     socket to connect to.
    \param gs       game status.
*/
static int send_data(int sock, struct game_state *gs)
{
    unsigned char data[FIELD_SIZE + 12] = {0};
    char *ptr = &(*gs->field)[0][0];

    data[0] = (unsigned char)gs->phase & 0xff;

    data[1] = gs->points & 0x000000ff;
    data[2] = gs->points & 0x0000ff00;
    data[3] = gs->points & 0x00ff0000;
    data[4] = gs->points & 0xff000000;

    data[5] = gs->level & 0x000000ff;
    data[6] = gs->level & 0x0000ff00;
    data[7] = gs->level & 0x00ff0000;
    data[8] = gs->level & 0xff000000;

    data[9] = gs->togo & 0x000000ff;
    data[10] = gs->togo & 0x0000ff00;
    data[11] = gs->togo & 0x00ff0000;
    data[12] = gs->togo & 0xff000000;

    for(size_t i = 0; i < FIELD_SIZE; i++)
    {
        data[i + 13] = *ptr;
        ptr++;
    }
 
    if(send(sock, data, FIELD_SIZE + 16, 0) < 0)
    {
        perror("send()");
        return 1;
    }
    return 0;
}

/*! \brief This process is started by the child and handles the game session for each client.
    \param sock         socket to connect to the client.
    \param client_id    client id, or game session in use.
    \return 0 on success, 1 on error
*/
static int child_process(int sock, int client_id)
{
    char recv_data = 0;

    init_game(client_id);

    while(1)
    {
        struct game_state *gs = NULL;

        /* Move current block one column left or right */
        gs = handle_input(client_id, recv_data);
        if(gs == NULL)
        {
            return 1;
        }
        if(nanosleep(&(struct timespec){0, DELAY_MS*1000*1000}, NULL) != 0)
        {
            perror("nanosleep()");
            return 1;
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
            return 0;
        }
        if(send_data(sock, gs) != 0)
        {
            return 1;
        }
        if(recv(sock, &recv_data, 1, 0) < 0)
        {
            perror("recv()");
            return 1;
        }

        if(nanosleep(&(struct timespec){0, DELAY_MS*1000*1000}, NULL) != 0)
        {
            perror("nanosleep()");
            return 1;
        }
    }
    
    return 0;
}

/*! \brief Finish and cleanup everything.
    \param sig    signal which triggered this function.
*/
static void finish(int sig)
{
    printf("Bye!\n");
    // close socket and kill all running childrenss
    exit(sig == SIGINT ? 0 : sig);
}
