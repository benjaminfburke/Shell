
/*
 * shell.y: parser for shell
 *
 */

%code requires
{
#include <string>
#include <string.h>

#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN GREAT NEWLINE LESS AND PIPE GREATGREAT GREATAND GREATGREATAND TWOGREAT

%{
//#define yylex yylex
#include <cstdio>
#include "shell.hh"
#include "command.hh"
#include <regex.h>
#include <assert.h>
#include <string.h>
#include <dirent.h>

void yyerror(const char * s);
int yylex();

%}

%%

goal:
  commands
  ;

commands:
  command
  | commands command
  ;

command: simple_command
       ;

simple_command:
  pipe_list iomodifier_list background_opt NEWLINE {
    Shell::_currentCommand.execute();
  }
  | NEWLINE { Shell::prompt(); }
  | error NEWLINE { yyerrok; }
  ;

command_and_args:
  command_word argument_list {
    Shell::_currentCommand.
    insertSimpleCommand( Command::_currentSimpleCommand );
  }
  ;

argument_list:
  argument_list argument
  | /* can be empty */
  ;

argument:
  WORD {
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
  ;

command_word:
  WORD {
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
  ;

pipe_list:
  pipe_list PIPE command_and_args
  | command_and_args
  ;

iomodifier_opt:
  GREAT WORD {
    if (Shell::_currentCommand._outFile) {
      yyerror("Ambiguous output redirect.\n");
    }
    Shell::_currentCommand._outFile = $2;
  }
  | LESS WORD {
      Shell::_currentCommand._inFile = $2;
  }
  | GREATGREAT WORD {
      Shell::_currentCommand._outFile = $2;
      Shell::_currentCommand._append = true;
  }
  | GREATAND WORD {
      Shell::_currentCommand._outFile = $2;
      Shell::_currentCommand._errFile = $2;
  }
  | TWOGREAT WORD {
      Shell::_currentCommand._errFile = $2;
  }
  | GREATGREATAND WORD {
      Shell::_currentCommand._outFile = $2;
      Shell::_currentCommand._errFile = $2;
      Shell::_currentCommand._append = true;
  }
  ;

iomodifier_list:
  iomodifier_list iomodifier_opt
  |
  ;

background_opt:
  AND {
    Shell::_currentCommand._background = true;
  }
  |
  ;

%%

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}

#if 0
main()
{
  yyparse();
}
#endif
