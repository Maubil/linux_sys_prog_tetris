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
    char *server_ip = "127.0.0.1";
    char *server_port = "30001";
    SOCKET sock = 0;
    int check_port = 0;

    /* tetris_client [-i <server ip>] [-p <server port>] [-h]
    The value of the -p option specifies the TCP port of the socket that server shall listen to. 
    The client allows to specify the server IP address to connect to additionally to the destination port. 
    If the IP is missing 127.0.0.1 shall be used on default. Also, select a suitable default port number (anything but 31457…​ why?).

    If the -h option is given either program should display a sensible usage message explaining the syntax and exit afterwards. 
    An illegal IP or port address should also trigger the display of the usage message and a termination. 
    Both application should return with 0 on success or 1 if there are any errors (including but not limited to illegal IPs/ports). 
    Use getopt(3) to handle the arguments.
    */

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
static SOCKET init_connection(const char *server_ip, const char *server_port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s;
    SOCKET sfd;

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

int game_session(SOCKET sock)
{
    // Variables to store the previous and new terminal attributes
    static struct termios oldterm, newterm;

    // Save the old ones and disable canonical mode + echoing
    tcgetattr(0, &oldterm);
    newterm = oldterm;
    newterm.c_lflag &= ~ICANON;
    newterm.c_lflag &= ~ECHO;
    tcsetattr(0, TCSANOW, &newterm);

    while (1) 
    {
        char c = getchar();

        //write_server(sock, "Helloooooo");
        printf("Blocking?\n");
        if (c == 0x1B) 
        {
            // 0x1B ("ESC") indicates the start of an escape sequence

            // Store escape sequence in variable esc
            int esc = ((getchar() & 0xFF) << 8) | (getchar() & 0xFF);

            // Behave differently depending on which key was pressed
            switch (esc) {
                case 0x5B41: printf("up\n"); break;
                case 0x5B42: printf("down\n"); break;
                case 0x5B43: printf("right\n"); break;
                case 0x5B44: printf("left\n"); break;
            }
        } 
        else if (c == 0x7f) 
        {
            // 0x7f indicates backspace
            write_server(sock, "Helloooooo");
            // Move cursor one char left
            printf("\b");
            // Overwrite it with a space
            printf(" ");
            // Move cursor back again for new user input at "deleted" position
            printf("\b");
        } 
        else if (c == 'q') 
        {
            // Exit loop on 'q'
            break;
        } 
        else 
        {
            printf("%c", c);
        }
    }

    // Restore previous terminal settings on exit
    tcsetattr(0, TCSANOW, &oldterm);

    return 0;
}