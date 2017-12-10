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
#include "game.h"
#include "queues.h"
#include "common.h"

#define DELAY_MS (10)
#define NB_HIGH_SCORES_SHOWN (10)
#define HIGH_SCORE_FILE ("./high_scores.txt")

struct client_data_t {
    int socket;
    pthread_t thread;
};

static uint32_t high_score[NB_HIGH_SCORES_SHOWN] = {0};
static struct client_data_t worker_thread_data[CLIENTS_MAX];

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
        socklen_t client_addr_len=sizeof(clientaddr);
        int client_id = get_client_id();
        if(client_id != INVALID_CLIENT_ID)
        {
            (void)printf("Accepting connections on client id %d!\n", client_id);
            worker_thread_data[client_id].socket = accept(sockid, (struct sockaddr *)&clientaddr, &client_addr_len);

            /* start a new thread for each new client until we reach CLIENTS_MAX */
            if(pthread_create(&worker_thread_data[client_id].thread, NULL, child_thread, &client_id) != 0)
            {
                close(worker_thread_data[client_id].socket);
                perror("ptherad_create()");
                return 1;
            }
            printf("Thread started!\n");
        }
        else{
            printf("invalid client\n");
        }
    }

    return 0;
}

void *child_thread(void *ptr)
{
    int data = *(int*)ptr;

    int rc = child_process(worker_thread_data[data].socket, data);
    if(rc != 0 && rc != 2)
    {
        printf("child process died rc is %d!\n", rc);
        close(worker_thread_data[data].socket);
        exit(45);
    }
    printf("I was killed properly!\n");
    release_client_id(data);
    close(worker_thread_data[data].socket);
    return NULL;
}

void *high_score_writer_task(void *ptr) 
{
    char * line = NULL;
    size_t len = 0;
    size_t i = 0;
    uint32_t data_in = 0;

    (void)ptr;

    FILE *fp = fopen(HIGH_SCORE_FILE, "r");
    if(fp == NULL)
    {
        perror("fopen()");
        exit(1);
    }

    while ((getline(&line, &len, fp)) != -1 && i < NB_HIGH_SCORES_SHOWN) 
    {
        high_score[i++] = atoi(line);
    }

    fclose(fp);

    bubble_sort(high_score, NB_HIGH_SCORES_SHOWN);

    for(size_t i = 0; i < NB_HIGH_SCORES_SHOWN; i++)
    {
        printf("high score is %u\n", high_score[i]);
    }

    while(1)
    {
        if(consume(&data_in) != 0)
        {
            printf("ERROR");
            perror("consume error");
            break;
        }

        printf("GOT A MESSAGE, value is %d!\n", data_in);

        /* if the new value is at least bigger than the lowest high score entry */
        if(data_in > high_score[0])
        {
            printf("Adding to high score!\n");

            /* add value to the end and let bubble sort work */
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
 
    if(send(sock, data, FIELD_SIZE + 16, 0) < 0)
    {
        perror("send()");
        return 1;
    }
    return 0;
}

static int send_high_scores(int sock)
{
    char recv_data = 0;
    uint32_t scores[NB_HIGH_SCORES_SHOWN] = {0};

    for(size_t i = NB_HIGH_SCORES_SHOWN - 1; i != 0; i--)
    {
        scores[i] = htonl(high_score[i]);
    }

    if(send(sock, scores, sizeof(scores) / sizeof(scores[0]), 0) < 0)
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
    uint32_t last_handling = time_in_ms();

    /* do not die on broken pipes, but handle and return */
    (void)signal(SIGPIPE, SIG_IGN);

    if(send_high_scores(sock) != 0)
    {
        return 1;
    }

    (void)printf("Client %d is starting a new game!\n", client_id);
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
        if((time_in_ms() - last_handling) > STEP_TIME_GRANULARITY)
        {
            gs = handle_substep(client_id);
            last_handling = time_in_ms();
        }
        if(send_data(sock, gs) != 0)
        {
            return 1;
        }
        if (gs->phase == TET_LOSE || gs->phase == TET_WIN) 
        {
            puts("HERE I AM");
            printf("Player %s with %u points in level %u.\n",
                    gs->phase == TET_WIN ? "wins" : "loses", gs->points, gs->level);
            if(produce(gs->points) != 0)
            {
                perror("produce error");
                return 1;
            }
            return 0;
        }
        if(recv(sock, &recv_data, 1, 0) < 0)
        {
            perror("recv()");
            return 2;
        }
        if(nanosleep(&(struct timespec){0, DELAY_MS*1000*1000}, NULL) != 0)
        {
            perror("nanosleep()");
            return 1;
        }
        if(recv_data == 'q')
        { 
            break;
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

    exit(0);
}
