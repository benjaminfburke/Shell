#include <cstdio>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#include "shell.hh"

int yyparse(void);
void yyrestart(FILE * file);

void Shell::prompt() {
  //printf("myshell>");
  fflush(stdout);
}

extern "C" void ctrl_C(int sig) {
  if (sig == SIGINT) {
    printf("Exiting program\n");
    exit(EXIT_SUCCESS);
  }
}

extern "C" void zombie(int sig) {
  pid_t status = wait3(0, 0, NULL);
  while (waitpid(-1, NULL, WNOHANG) > 0) {
    printf("\n[%d] exited.", status);
  }
}

int main() {
  struct sigaction sigAction_ctrlC;
  sigAction_ctrlC.sa_handler = ctrl_C;
  sigemptyset(&sigAction_ctrlC.sa_mask);
  sigAction_ctrlC.sa_flags = SA_RESTART;

  if (sigaction(SIGINT, &sigAction_ctrlC, NULL)) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  struct sigaction sigAction_zombie;
  sigAction_zombie.sa_handler = zombie;
  sigemptyset(&sigAction_zombie.sa_mask);
  sigAction_zombie.sa_flags = SA_RESTART;

  if (sigaction(SIGCHLD, &sigAction_zombie, NULL)) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  FILE * file = fopen(".shellrc", "r");
  if (file) {
    yyrestart(file);
    yyparse();
    yyrestart(stdin);
    fclose(file);
  }
  else {
    if (isatty(0)) {
      Shell::prompt();
    }
  }
  yyparse();
}

Command Shell::_currentCommand;
