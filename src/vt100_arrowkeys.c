#include <stdio.h>
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