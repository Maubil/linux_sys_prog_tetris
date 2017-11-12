# linux_sys_prog_tetris
# Assignment
Below you will find the specification of the project. It deals with many topics covered in the lecture. You are required to send your solution as an archive (e.g., .zip) with your surname as filename to Stefan Tauner by e-mail. The archive should only contain the required source files, a makefile and a README.txt or similar document. The README file needs to list any optional parts you have implemented and shall contain other important information like additional command line parameters, usage information etc. if need be. Additionally, you have to defend your solution in a short exercise interview at the end of the course.

# Introduction
You shall implemented a distributed “Tetris” client using TCP/IP sockets.

The game logic is to implemented by a single server application that opens a listening port (anything but 31457). Every connecting client shall immediately start a new game. The client is responsible to display the play field and forward user input to the server. The server shall process the input, simulate the next time step of the game, and return the new output (play field, current score, etc.) to the client. You may use the game logic provided down below or implement your own (see extras). The game logic shall continue to be processed even without user inputs/updates from the client. A good way to implement this behavior is a forking server that creates one server process per new connection/client (see also the “Multiprocessing” and “Multithreading” extras below).

The common play field size is 10 columns × 18 rows. To avoid problems, your benevolent lecturer recommends to use ASCII characters only for the display instead of UTF-8 or similar encodings. Alternatively, you may use a dedicated library such as ncurses (see later).

At the beginning of each game, the current ten highest scores should be displayed on the client. The values of this are stored by the server process in a file and are thus made persistent. The server needs to update the file if need be after each finished game.

# Invocation
The server has to be started by using the syntax listed below.
tetris_server [-p <port>] [-h]

The client should be executed identically like so:
tetris_client [-i <server ip>] [-p <server port>] [-h]

The value of the -p option specifies the TCP port of the socket that server shall listen to. The server shall listen on all possible interfaces in case there are more than one. The client allows to specify the server IP address to connect to additionally to the destination port. If the IP is missing 127.0.0.1 shall be used on default. Also, select a suitable default port number (anything but 31457…​ why?).

If the -h option is given either program should display a sensible usage message explaining the syntax and exit afterwards. An illegal IP or port address should also trigger the display of the usage message and a termination. Both application should return with 0 on success or 1 if there are any errors (including but not limited to illegal IPs/ports). Use getopt(3) to handle the arguments.

The client accepts commands in a suitable way from its standard input. The table below is meant as a suggestion.

Command/Key	Description
←, →    Moves the currently falling block one step to the left/right.
↑       Rotates the currently falling block about +90°.
↓       Moves the currently falling block one line down.
q       Quits the connection to the server and shuts the client down.
r       Restarts a game.
[space] Moves the current block immediately to the lowest possible position instead of letting it fall freely stepwise.

There should be appropriate ways to shut down the server and clients, e.g., by sending a signal (most suitable for the server) or text input (q in the client). At shutdown all resources like opened files and sockets, allocated memories etc. should be returned to the system. Also, the communication partner(s) should be informed and react accordingly.

# Terminal Interaction
For interactive programs it is often useful to handle input differently than in ordinary applications. This can be accomplished by using tcsetattr(3) to manipulate the respective terminal attributes. Specifically, one may want to disable echoing of entered characters and handle special keys like cursor left/right or backspace directly within the application instead of relying on the terminal.

When exiting the terminal the old settings need to be applied again manually or the user is left with a very odd-behaving command prompt (i.e., the attributes remain set even after the application exits). One can reinitialize the terminal’s settings by calling reset(1) (“blindly”).

The example below shows how to react to arrow keys and how backspace can be implemented manually. To do so, one has to parse the multi-byte long input strings - the so-called ANSI/VT100 Escape Codes.
[vt100_arrowkeys.c](https://cis.technikum-wien.at/documents/mes/1/sec/semesterplan/em2/listings/vt100_arrowkeys.c)

The other direction is also possible: by emitting certain escape sequences a program can control the terminal. For example, using puts("\033[2J"); clears the current screen similar to what the execution of clear(1) through system(3) would do.

More comfortable is the use of a fully-fledged library designed for these purposes such as ncurses (cf. Wikipedia, Howto by one of the ncurses authors, TLDP Howto).

# Game Logic Implementation
If you want to avoid the fun of implementing the game logic itself and concentrate on the system programming aspects you may use the code below. The file game.h specifies the interface to the respective module game.c and needs to be integrated into your project. game_test.c briefly shows how its functions need to be called to make progress in the game (although in a nonsensical way).
[game.h](https://cis.technikum-wien.at/documents/mes/1/sec/semesterplan/em2/listings/game.h)
[game.c](https://cis.technikum-wien.at/documents/mes/1/sec/semesterplan/em2/listings/game.c)
[game_test.c](https://cis.technikum-wien.at/documents/mes/1/sec/semesterplan/em2/listings/game_test.c)

# Grading
Initially, you will receive 80 points. However, various amounts of points will be deducted for failing to fulfill the following constraints.

-100 Points: The application has to implement all mandatory features as listed in the specification.
-30 Points: The application does only use functionality provided by C99 (or later) and POSIX.1-2008 (or later) apart from explicit exceptions stated in the specification.
-30 Points: The application should never crash, behave erratically or hang in my test environment due to programming errors (NULL pointer dereferences, missing synchronization, undefined C behavior etc). Experience shows that focusing on testing instead of optional features will probably lead to a better grade - this is on purpose.
-20 Points: The delivery shall be exactly as described above and contain no superfluous binary files such as generated object files or application (but images or other data files required by your program are fine of course).
-20 Points: The modules of your program have to compile without warnings using the options -std=c99 -Wall -pedantic in gcc and clang.
-20 Points: You have to supply a proper Makefile whose default target builds all binaries of the project. Additionally, there has to be a clean target that removes all generated files.
-10 Points: Implement your program in sensible modules.
-10 Points: Split your program in a reasonable amount of functions. Avoid spaghetti code.
-10 Points: Every function must be documented above its header with a brief description of the respective function, its parameters and return values. You may use Doxygen (optional).
-1 Point for each occurrence: Check the return values of each function and do something meaningful with them (if ignoring them is the most sensible option you optionally may cast the result value to void (e.g., printf(), fprintf(stdout,…​))).
-1 Point for each occurrence: Print meaningful error messages upon failure of a function. Use perror() and strerror() where applicable.
-pow(2, d) Points where d is the number of commenced days after the official deadline (e.g., deadline 1.1. 09:00; delivery 1.1. 10:00: -2 points, delivery 4.1. 9:00: -8 points).

The deadline for the submission of your solution of the project is 2017-12-03 00:00 (neither 23:59 nor 24:00).

Additional points might be obtained (or lost) for extensive participation in class and depending on the answers given in the final exercise interview. Also, if there are some optional parts in the project specification that you implement correctly they add points to your account. You will get one extra point up to a maximum of 5 for each error in the lecture notes or slides that you report to me by e-mail (if I am not aware of them yet).

# Warning!
Discussion among students about any problems as well as approaches to implement the project is encouraged. However, copying foreign code without proper attribution is forbidden and will be prosecuted by failing the course. If you use some code snippets from the net or colleagues they must be clearly marked and named in the source. Of course that also does not mean that you should simply copy a solution - the vast majority of the code has to be your own. You have to be prepared to present and explain your solution in detail - including any foreign code you use.