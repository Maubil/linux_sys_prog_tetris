#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <netdb.h>

static void print_usage(const char *prog_name);
static void find_server_ip(const char *ip_addr);

int main(int argc, char *argv[])
{
    char c = 0;
    char *server_ip = "127.0.0.1";
    int server_port = 30001;

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
            case 'i':
                /* user passed server IP */
                server_ip = optarg;
                break;

            case 'p':
                /* user passed server port */
                server_port = atoi(optarg);
                /* only allow user ranged ports */
                if(server_port >= 65535 || server_port <= 1024)
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
    find_server_ip(server_ip);
    (void)server_port;

    return 0;
}

static void find_server_ip(const char *ip_addr)
{
    struct addrinfo *servinfo = NULL;
    int ret = getaddrinfo(ip_addr, NULL, NULL, &servinfo);
    if(ret != 0)
    {
        exit(EXIT_FAILURE);
    }
    
    /* loop through all DNS results */
    struct addrinfo *p = NULL;
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        char ipstr[8*4 + 7 + 1] = "";

        if (p->ai_family == AF_INET)
        {
            struct sockaddr_in *s = NULL;
        }

    }
    
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



/* #include <stdio.h>
#include <termios.h>

int main (void) {
  // Variables to store the previous and new terminal attributes
  static struct termios oldterm, newterm;

  // Save the old ones and disable canonical mode + echoing
  tcgetattr(0, &oldterm);
  newterm = oldterm;
  newterm.c_lflag &= ~ICANON;
  newterm.c_lflag &= ~ECHO;
  tcsetattr(0, TCSANOW, &newterm);

  while (1) {
    char c = getchar();

    if (c == 0x1B) {
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
    } else if (c == 0x7f) {
      // 0x7f indicates backspace

      // Move cursor one char left
      printf("\b");
      // Overwrite it with a space
      printf(" ");
      // Move cursor back again for new user input at "deleted" position
      printf("\b");
    } else if (c == 'q') {
      // Exit loop on 'q'
      break;
    } else {
        printf("%c", c);
    }
  }

  // Restore previous terminal settings on exit
  tcsetattr(0, TCSANOW, &oldterm);

  return 0;
}
*/