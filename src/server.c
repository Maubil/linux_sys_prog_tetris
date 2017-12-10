#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include "game.h"
#include "queues.h"
#include "common.h"

#define DELAY_MS (10)
#define NB_HIGH_SCORES_SHOWN (10)
#define HIGH_SCORE_FILE ("./high_scores.txt")

struct client_data_t {
    int socket;
    uint32_t id;
    pthread_t thread;
};

static uint32_t high_score[NB_HIGH_SCORES_SHOWN] = {0};

static void print_usage(const char *prog_name);
static int send_data(int sock, struct game_state *gs);
static int child_process(int sock, uint32_t client_id);
static void finish(int sig);
static int send_high_scores(int sock);
void *high_score_writer_task(void *ptr);
void *child_thread(void *ptr);

int main(int argc, char *argv[])
{
    char c = 0;
    int sockid = 0;
    int check_port = 30001;
    pthread_t thread1;
    struct client_data_t worker_thread_data[CLIENTS_MAX];
    struct sockaddr_in myaddr, clientaddr;

    (void) signal(SIGINT, finish);
    if(init_queue() != 0)
    {
        perror("pthread error");
        return 1;
    }
    int iret1 = pthread_create(&thread1, NULL, high_score_writer_task, NULL);
    if(iret1 != 0)
    {
        perror("ptherad_create()");
        return 1;
    }

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
    myaddr.sin_addr.s_addr=htonl(INADDR_ANY);
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
        socklen_t client_addr_len=sizeof(clientaddr);
        int client_id = get_client_id();
        if(client_id != INVALID_CLIENT_ID)
        {
            (void)printf("Accepting connections on client id %d!\n", client_id);
            worker_thread_data[client_id].id = client_id;
            worker_thread_data[client_id].socket = accept(sockid, (struct sockaddr *)&clientaddr, &client_addr_len);

            /* start a new thread for each new client until we reach CLIENTS_MAX */
            if(pthread_create(&worker_thread_data[client_id].thread, NULL, child_thread, &worker_thread_data[client_id]) != 0)
            {
                close(worker_thread_data[client_id].socket);
                perror("ptherad_create()");
                return 1;
            }
        }
        else{
            (void)printf("no more clients available...\n");
        }
    }

    return 0;
}

void *child_thread(void *ptr)
{
    struct client_data_t *data = (struct client_data_t*)ptr;

    int rc = child_process(data->socket, data->id);
    close(data->socket);
    release_client_id(data->id);

    if(rc != 0 && rc != 2)
    {
        exit(1);
    }
    
    return NULL;
}

void *high_score_writer_task(void *ptr) 
{
    char * line = NULL;
    size_t len = 0;
    size_t i = 0;
    uint32_t data_in = 0;

    (void)ptr;

    /* read high score file, sort data and save them to memory */
    FILE *fp = fopen(HIGH_SCORE_FILE, "r");
    if(fp == NULL)
    {
        perror("fopen()");
        exit(1);
    }

    while (getline(&line, &len, fp) != -1 && i < NB_HIGH_SCORES_SHOWN) 
    {
        high_score[i++] = atoi(line);
    }

    fclose(fp);

    bubble_sort(high_score, NB_HIGH_SCORES_SHOWN);

    while(1)
    {
        if(consume(&data_in) != 0)
        {
            perror("consume error");
            break;
        }

        /* if the new value is at least bigger than the lowest high score entry */
        if(data_in > high_score[0])
        {
            /* add value and let bubble sort work */
            high_score[0] = data_in;
            bubble_sort(high_score, NB_HIGH_SCORES_SHOWN);
        }
    }

    return NULL;
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

/*! \brief serialize and send game data to client.
    \param sock     socket to connect to.
    \param gs       game status.
*/
static int send_data(int sock, struct game_state *gs)
{
    char data[FIELD_SIZE + 16] = {0};

    serialize_data(data, gs);
 
    if(send(sock, data, FIELD_SIZE + 16, MSG_NOSIGNAL) < 0)
    {
        perror("send()");
        return 1;
    }
    return 0;
}

static int send_high_scores(int sock)
{
    char recv_data = 0;
    char scores[NB_HIGH_SCORES_SHOWN * 4] = {0};

    for(size_t i = NB_HIGH_SCORES_SHOWN - 1; i != 0; i--)
    {
        scores[(i + 1) * 4 - 1] = (char)high_score[(i + 1) - NB_HIGH_SCORES_SHOWN];
        scores[(i + 1) * 4 - 2] = (char)(high_score[(i + 1) - NB_HIGH_SCORES_SHOWN] >> 8);
        scores[(i + 1) * 4 - 3] = (char)(high_score[(i + 1) - NB_HIGH_SCORES_SHOWN] >> 16);
        scores[(i + 1) * 4 - 4] = (char)(high_score[(i + 1) - NB_HIGH_SCORES_SHOWN] >> 24);
    }

    if(send(sock, scores, NB_HIGH_SCORES_SHOWN * 4, MSG_NOSIGNAL) < 0)
    {
        perror("send()");
        return 1;
    }

    /* block and wait on data from user to continue */
    if(recv(sock, &recv_data, 1, 0) < 0)
    {
        perror("recv()");
        return 1;
    }

    return 0;
}

/*! \brief This process is started by the child and handles the game session for each client.
    \param sock         socket to connect to the client.
    \param client_id    client id, or game session in use.
    \return 0 on success, 1 on error
*/
static int child_process(int sock, uint32_t client_id)
{
    char recv_data = 0;
    int rc = 1;
    struct game_state *gs = NULL;
    uint32_t last_handling = time_in_ms();

    /* do not die on broken pipes, but handle and return */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror(0);
        exit(1);
    }

    if(send_high_scores(sock) != 0)
    {
        return 1;
    }

    (void)printf("Client %d is starting a new game!\n", client_id);
    init_game(client_id);

    while(1)
    {
        gs = handle_input(client_id, recv_data);
        if(gs == NULL)
        {
            rc = 1; break;
        }
        if(nanosleep(&(struct timespec){0, DELAY_MS*1000*1000}, NULL) != 0)
        {
            perror("nanosleep()");
            rc = 1; break;
        }
        if((time_in_ms() - last_handling) > STEP_TIME_GRANULARITY)
        {
            gs = handle_substep(client_id);
            last_handling = time_in_ms();
        }
        if(send_data(sock, gs) != 0)
        {
            rc = 2; break;
        }
        if (gs->phase == TET_LOSE || gs->phase == TET_WIN) 
        {
            (void)printf("Player %s with %u points in level %u.\n",
                    gs->phase == TET_WIN ? "wins" : "loses", gs->points, gs->level);
            rc = 0; break;
        }
        if(recv(sock, &recv_data, 1, 0) < 0)
        {
            perror("recv()");
            rc = 2; break;
        }
        if(nanosleep(&(struct timespec){0, DELAY_MS*1000*1000}, NULL) != 0)
        {
            perror("nanosleep()");
            rc = 1; break;
        }
        if(recv_data >= TET_MAX)
        {
            (void)printf("User sent unknown character!\n");
            rc = 2; break;
        }
    }
    if(produce(gs->points) != 0)
    {
        perror("produce error");
        rc = 1;
    }
    return rc;
}

/*! \brief Finish and cleanup everything.
    \param sig    signal which triggered this function.
*/
static void finish(int sig)
{
    (void)sig;
    FILE *fp = fopen(HIGH_SCORE_FILE, "w+");
    if(fp == NULL)
    {
        perror("fopen()");
        exit(1);
    }

    for(size_t i = 0; i < NB_HIGH_SCORES_SHOWN; i++)
    {
        (void)fprintf(fp, "%u\n", high_score[i]);
    }

    fclose(fp);

    (void)cleanup_queue();

    (void)printf("Data saved. Exiting!!\n");

    exit(0);
}
