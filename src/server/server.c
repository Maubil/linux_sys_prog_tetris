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

#define BUF_SIZE 255

typedef int SOCKET;

static void print_usage(const char *prog_name);
static SOCKET init_connection(const char *server_ip, const char *server_port);
static int read_server(SOCKET sock, char *buffer);
static void write_server(SOCKET sock, const char *buffer);
int game_session(SOCKET sock);

int main(int argc, char *argv[])
{
    char c = 0;
    char *server_port = "30001";
    SOCKET sock = 0;
    int check_port = 0;

    /* tetris_client [-i <server ip>] [-p <server port>] [-h]
    The value of the -p option specifies the TCP port of the socket that server shall listen to. 
    The server shall listen on all possible interfaces in case there are more than one. 
    The client allows to specify the server IP address to connect to additionally to the destination port. 
    If the IP is missing 127.0.0.1 shall be used on default. Also, select a suitable default port number (anything but 31457…​ why?).

    If the -h option is given either program should display a sensible usage message explaining the syntax and exit afterwards. 
    An illegal IP or port address should also trigger the display of the usage message and a termination. 
    Both application should return with 0 on success or 1 if there are any errors (including but not limited to illegal IPs/ports). 
    Use getopt(3) to handle the arguments.
    */

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

    close(sock);
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

static void write_server(SOCKET sock, const char *buffer)
{
    if(send(sock, buffer, strlen(buffer), 0) < 0)
    {
        perror("send()");
        exit(errno);
    }
}
