
/*
 *
 * shell.l: lexical analyzer for shell
 *
 */

%{

#include <cstring>
#include "y.tab.hh"
#include <iostream>
#include <sys/wait.h>
#include <string>
#include "shell.hh"
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <vector>

///////// start added code /////////

extern "C" char * read_line();

int mygetc(FILE * f) {
  static char *p;
  char ch;

  if (!isatty(0)) {
    //stdin is not a tty. call real getc
    return getc(f);
  }

  if (p == NULL || *p == 0) {
    //stdin is a tty. call our read_line
    char *s = read_line();
    p = s;
  }

  ch = *p;
  p++;

  return ch;
}

#undef getc
#define getc(f) mygetc(f)

///////// end added code /////////

static  void yyunput (int c,char *buf_ptr  );

extern int r;
extern int exclaim;
extern std::string last;

void myunputc(int c) {
  unput(c);
}

void src(const char * filename) {
  FILE * fp = fopen(filename, "r");
  //create buffer
  YY_BUFFER_STATE buffer = yy_create_buffer(fp, YY_BUF_SIZE);
  yypush_buffer_state(buffer); //get state of buffer
  yyparse();
  yypop_buffer_state();
}

%}

%option noyywrap

%%

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

">" {
  return GREAT;
}

"<" {
  return LESS;
}

"&" {
  return AND;
}

"|" {
  return PIPE;
}

">>" {
  return GREATGREAT;
}

">>&" {
  return GREATGREATAND;
}

">&" {
  return GREATAND;
}

"2>" {
  return TWOGREAT;
}

~[^ \n\t]* {
  /* tilde expansion */

  //get text
  std::string text = std::string(yytext);

  if (text.size() == 1) { //if text is just the tilde
    yylval.cpp_string = new std::string(getenv("HOME"));
  }
  else { //if text contains more than just tilde
    text = std::string("/homes/") + text.substr(1, -1);
    yylval.cpp_string = new std::string(text);
  }

  return WORD;
}

\$\{[^\t\n ]*\}([^ \t\n])* {
  /* environment variable expansion */

  //get text
  std::string text = std::string(yytext);
  std::size_t found = text.find('$'); //find the $
  std::string end = "";
  std::string temp = "";

  //loop through until $ is removed
  while (found != -1) {
    std::string s;
    text.erase(found, 1);
    s += text;

    if (text[found] == '{') { //open bracket
      text.erase(found, 1);
      std::size_t bracket = text.find('}'); //find the closed bracket
      temp = text.substr(0, bracket); //get the first

      if (bracket != -1) {
        text.erase(0, bracket + 1);
      }
      if (getenv(temp.c_str()) != NULL) {
        end += getenv(temp.c_str());
      }
      if (temp == "$") { //the pid of the shell process
        end += std::to_string(getpid());
      }
      if (temp == "!") { //the pid of the last process run in the background
        end += std::to_string(exclaim);
      }
      if (temp == "?") { //r code
        end = std::to_string(r);
      }
      if (temp == "_") { //last
        end = last;
      }
    }

    //update found
    found = text.find('$');
    end += text.substr(0, found);
    text.erase(0, found);
    found = text.find('$');
  }

  yylval.cpp_string = new std::string(end);

  return WORD;


}

$\([^\n]*\) {
  /* subshell */

  //get text
  std::string text = std::string(yytext);

  //remove ( ) and $
  text = text.substr(2, text.size() - 3);

  //end the string
  text = text.append("\nexit\n");

  //create defualt variables
  int defaultIn = dup(0);
  int defaultOut = dup(1);

  //create pipes
  int pipeIn[2], pipeOut[2];
  pipe(pipeIn);
  pipe(pipeOut);

  //redirection
  dup2(pipeIn[0], 0);
  dup2(pipeOut[1], 1);

  close(pipeIn[0]);
  close(pipeOut[1]);

  write(pipeIn[1], text.c_str(), text.size()); //write
  close(pipeIn[1]);

  //start forking process
  int pid = fork();

  if (pid == 0) { //child process
    char ** args = new char*[2];
    args[0] = (char*)"/proc/self/exe";
    args[1] = NULL; //null terminator

    //execute the command
    execvp(args[0], args);
    perror("execvp(subshell)");
    exit(1);
  }
  else if (pid < 0) { //fork failed
    perror("fork");
    exit(1);
  }

  //restore defaults and close file descriptors
  dup2(defaultIn, 0);
  dup2(defaultOut, 1);
  close(defaultIn);
  close(defaultOut);

  char * buff = new char[2048];
  int n = read(pipeOut[0], buff, 2048);
  close(pipeOut[0]);

  //put the char back
  for (int i = n - 1; i >= 0; --i) {
    if (buff[i] == '\n') {
      buff[i] = ' ';
    }
    myunputc(buff[i]);
  }
}

\"[^\n"]*\" {
  /* Support for quotes */
  std::string quotes = std::string(yytext);
  quotes = quotes.substr(1, quotes.length() - 2); //remove quote chars

  yylval.cpp_string = new std::string(quotes);


  return WORD;
}

[^ \\\n]*(\\[^nt])((\\[^nt])|[^ \\\n])* {
  /* escape character */
  std::string escape = std::string(yytext);
  std::size_t check = escape.find('\\');

  //could be more than one
  while (check != -1) {
    escape = escape.substr(0, check) + escape.substr(check + 1); //remove char at check position
    check = escape.find('\\', check + 1); //reset check with new starting point
  }

  yylval.cpp_string = new std::string(escape);

  return WORD;

}

[^ \t\n|><][^ \t\n|><]*  {
  /* Assume that file names have only alpha chars */
  yylval.cpp_string = new std::string(yytext);

  return WORD;
}


