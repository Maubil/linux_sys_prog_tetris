#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include "game.h"

#define INVALID_CLIENT_ID (-1)
#define DELAY_MS (10)
#define TICKS_PER_MS    (CLOCKS_PER_SEC / 1000)
#define NB_HIGH_SCORES_SHOWN (10)
#define MQKEY 0x42424242
#define MQTYPE 1
#define MQMAX_LEN 8

struct msg {
  long type;
  uint32_t score;
};

static bool clients_in_use[CLIENTS_MAX] = {0};
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static bool kill_signal = false;
static bool write_task_died = false;
static uint32_t high_score[NB_HIGH_SCORES_SHOWN] = {0};
static pthread_mutex_t high_score_mutex = PTHREAD_MUTEX_INITIALIZER;

static void print_usage(const char *prog_name);
static int send_data(int sock, struct game_state *gs);
static int child_process(int sock, int client_id);
static void finish(int sig);
static int get_client_id(void);
static void release_client_id(uint32_t client_id);
static int send_high_scores(int sock);
uint32_t time_in_ms(void);
void bubble_sort(uint32_t list[], long n);
void *high_score_writer_task(void *ptr);

int main(int argc, char *argv[])
{
    char c = 0;
    int sockid = 0;
    int check_port = 30001;
    pthread_t thread1;
    struct sockaddr_in myaddr, clientaddr;

    (void) signal(SIGINT, finish);
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
                (void)printf("Client %d is starting a new session!\n", client_id);
                child_process(new, client_id);
                release_client_id(client_id);
                (void)printf("Client %d finished its game session!\n", client_id);
            }
            else
            {
                (void)printf("no available session");
            }
            close(new);
            break;
        }
        else
        {
            close(new);
            continue;
        }
    }

    return 0;
}


void *high_score_writer_task(void *ptr) 
{
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    size_t i = 0;

    (void)ptr;

    FILE *fp = fopen("./high_scores.txt", "w+");
    if(fp == NULL)
    {
        write_task_died = true;
        perror("fopen()");
        exit(1);
    }
    if(pthread_mutex_lock(&high_score_mutex) != 0)
    {
        fclose(fp);
        exit(1);
    }

    while ((read = getline(&line, &len, fp)) != -1 && i < NB_HIGH_SCORES_SHOWN) 
    {
        high_score[i++] = atoi(line);
    }

    bubble_sort(high_score, NB_HIGH_SCORES_SHOWN);

    if(pthread_mutex_unlock(&high_score_mutex) != 0)
    {
        fclose(fp);
        exit(1);
    }

    long int id = msgget(MQKEY, 0);
    if (id < 0) 
    {
        perror("msgget");
        fclose(fp);
        exit(1);
    }

    while(kill_signal == false)
    {
        struct msg m;

        if (msgrcv(id, &m, sizeof(m.score), MQTYPE, 0) < 0) 
        {
            perror("msgrcv");
            break;
        }

        /* if the new value is at least bigger than the lowest high score entry */
        if(m.score > high_score[NB_HIGH_SCORES_SHOWN - 1])
        {
            if(pthread_mutex_lock(&high_score_mutex) != 0)
            {
                perror("pthread()");
                break;
            }

            /* add value to the end and let bubble sort work */
            high_score[NB_HIGH_SCORES_SHOWN - 1] = m.score;
            bubble_sort(high_score, NB_HIGH_SCORES_SHOWN);

            if(pthread_mutex_unlock(&high_score_mutex) != 0)
            {
                perror("pthread()");
                break;
            }
        }
    }

    for(size_t j = 0; j < NB_HIGH_SCORES_SHOWN; j++)
    {
        (void)fprintf(fp, "%u\n", high_score[j]);
    }

    fclose(fp);
    finish(0);
    return NULL;
}

/*
    /remark from: http://www.programmingsimplified.com/c/source-code/c-program-bubble-sort
*/
void bubble_sort(uint32_t list[], long n)
{
    uint32_t c, d, t;

    for (c = 0 ; c < ( n - 1 ); c++)
    {
        for (d = 0 ; d < n - c - 1; d++)
        {
            if (list[d] > list[d+1])
            {
            /* Swapping */
            t         = list[d];
            list[d]   = list[d+1];
            list[d+1] = t;
            }
        }
    }
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
    for(size_t i = 0; i < CLIENTS_MAX; i++)
    {
        if(clients_in_use[i] == false)
        {
            next_client_id = i;
            clients_in_use[i] = true;
        }
    }
    if(pthread_mutex_unlock(&mutex) != 0)
    {
        exit(1);
    }

    return next_client_id;
}

/*! \brief release a client id.
    \param client_id    id to release.
*/
static void release_client_id(uint32_t client_id)
{
    if(client_id >= CLIENTS_MAX)
    {
        return;
    }
    if(pthread_mutex_lock(&mutex) != 0)
    {
        exit(1);
    }
    clients_in_use[client_id] = false;
    if(pthread_mutex_unlock(&mutex) != 0)
    {
        exit(1);
    }
}

/*! \brief serialize and send game data to client.
    \param sock     socket to connect to.
    \param gs       game status.
*/
static int send_data(int sock, struct game_state *gs)
{
    char data[FIELD_SIZE + 16] = {0};
    char *ptr = &(*gs->field)[0][0];

    data[0] = (char)gs->phase;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;

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

    for(size_t i = 0; i <= FIELD_SIZE; i++)
    {
        data[i + 16] = *ptr;
        ptr++;
    }
 
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

    // TODO add high scores
    // high_score[i] = htonl(); ...

    if(send(sock, high_score, NB_HIGH_SCORES_SHOWN * sizeof(uint32_t), 0) < 0)
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
static int child_process(int sock, int client_id)
{
    char recv_data = 0;
    uint32_t last_handling = time_in_ms();

    send_high_scores(sock);

    (void)printf("Client %d is starting a new game!\n", client_id);
    long int id = msgget(MQKEY, IPC_CREAT | S_IRWXU | S_IROTH);
    if (id < 0) {
        perror("msgget");
        return 1;
    }
    init_game(client_id);

    while(kill_signal == false)
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
            struct msg m;
            fprintf(stderr, "Player %s with %u points in level %u.\n",
                    gs->phase == TET_WIN ? "wins" : "loses", gs->points, gs->level);
            
            m.type = MQTYPE;
            m.score = gs->points;
            if (msgsnd(id, &m, sizeof(m.score), 0) < 0) 
            {
                perror("msgsnd");
                return 1;
            }
            return 0;
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
    bool client_finished = false;
    printf("Bye!\n");
    kill_signal = true;
    // close socket and kill all running children

    /* wait 1s for childrens to terminate */
    uint32_t start_time = time_in_ms();
    while(time_in_ms() - start_time < 1000 || client_finished == false)
    {
        client_finished = true;
        if(pthread_mutex_lock(&mutex) != 0)
        {
            exit(1);
        }
        for(size_t i = 0; i < CLIENTS_MAX; i++)
        {
            if(clients_in_use[i] == true)
            {
                client_finished = false;
            }
        }
        if(pthread_mutex_unlock(&mutex) != 0)
        {
            exit(1);
        }
        if(nanosleep(&(struct timespec){0, DELAY_MS*1000*1000}, NULL) != 0)
        {
            perror("nanosleep()");
            exit(1);
        }
    }

    exit(sig == SIGINT ? 0 : sig);
}

/*! \brief Get the actual time and convert it into milliseconds.
    \return The actual time in ms.
*/
uint32_t time_in_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (uint32_t)(((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}