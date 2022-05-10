#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "command.hh"
#include "shell.hh"

void src(const char *);

int r = 0;
int exclaim = 0;
std::string last = "";


Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    _append = false;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    int flag = 1;

    if ( _outFile == _errFile ) {
        delete _errFile;
        flag = 0;
    }
    if ( _outFile && flag == 1 ) {
      delete _outFile;
    }
    _outFile = NULL;

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile && flag == 1 ) {
        delete _errFile;
    }
    _errFile = NULL;

    _background = false;

    _append = false;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}

void Command::execute() {
    // Don't do anything if there are no simple commands
    if ( _simpleCommands.size() == 0 ) {
        Shell::prompt();
        return;
    }

    //exit command
    if (strcmp(_simpleCommands[0]->_arguments[0]->c_str(), "exit") == 0) {
      printf("Good bye!!\n");
      exit(1);
    }

    // Print contents of Command data structure
    //print();

    // Add execution here
    // For every simple command fork a new process
    // Setup i/o redirection
    // and call exec

    //create temp variables
    int defaultIn = dup(0);
    int defaultOut = dup(1);
    int defaultErr = dup(2);

    int fdin;
    int fdout;
    int fderr;

    //generate the in file
    if (_inFile) {
      const char * in = _inFile->c_str();
      fdin = open(in, O_RDONLY, 0600); //only read the input file
    }
    else { //default input
      fdin = dup(defaultIn);
    }

    int pid;
    for (size_t i = 0; i < _simpleCommands.size(); i++) {
      dup2(fdin, 0); // input comes from fdin
      close(fdin);
      if (i == _simpleCommands.size() - 1) { //last command

        //generate out file
        if (_outFile) {
          const char * out = _outFile->c_str();
          if (_append == true) { //check append flag
            fdout = open(out, O_WRONLY | O_CREAT | O_APPEND, 0600); //can append
          }
          else {
            fdout = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0600); //can't append
          }
        }
        else { //default
          fdout = dup(defaultOut);
        }

        //generate error file
        if (_errFile) {
          const char * err = _errFile->c_str();
          if (_append == true) { //check append flag
            fderr = open(err, O_WRONLY | O_CREAT | O_APPEND, 0600); //can append
          }
          else {
            fderr = open(err, O_WRONLY | O_CREAT | O_TRUNC, 0600); //can't append
          }
        }
        else { //default
          fderr = dup(defaultErr);
        }
      }
      else { //not the last command
        int fdpipe[2];
        pipe(fdpipe);
        fdin = fdpipe[0];
        fdout = fdpipe[1];
      }

      dup2(fdout, 1);
      dup2(fderr, 2);
      close(fdout);
      close(fderr);

      //cd command
      if (!strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "cd")) {
        int ret_val;
        if (_simpleCommands[i]->_arguments.size() == 1) {
          ret_val = chdir(getenv("HOME"));
        }
        else {
          ret_val = chdir(_simpleCommands[i]->_arguments[1]->c_str());
        }

        //cd can't be found
        if (ret_val != 0) {
          fprintf(stderr, "cd: can't cd to notfound\n");
        }
        continue;

      }
      //setenv command
      else if (!strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "setenv")) {
        setenv(_simpleCommands[i]->_arguments[1]->c_str(), _simpleCommands[i]->_arguments[2]->c_str(), 1);
        continue;
      }
      //unsetenv command
      else if (!strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "unsetenv")) {
        unsetenv(_simpleCommands[i]->_arguments[1]->c_str());
        continue;
      }
      //source command
      else if (!strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "source"))  {
        _simpleCommands.clear();
        src(_simpleCommands[i]->_arguments[1]->c_str());
        return;
      }


      //get global var from parent, not child
      char **temp = environ;

      //start the process
      pid = fork();
      if (pid == 0) { //child process

        //printenv builtin function
        if (!strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "printenv")) {
          char ** env = temp;
          while (*env != NULL) {
            printf("%s\n", *env);
            env++;
          }
          exit(0);
        }

        char * simArray[_simpleCommands[i]->_arguments.size() + 1];
        //iterate through the arguments
        size_t x;
        for (x = 0; x < _simpleCommands[i]->_arguments.size(); x++) {
          simArray[x] = strdup(_simpleCommands[i]->_arguments[x]->c_str());
        }
        simArray[x] = NULL;
        execvp(simArray[0], simArray);
        perror("execvp");
        exit(1);
      }
      else if (pid == -1) { //fork failed
        perror("fork");
        return;
      }

      last = strdup(_simpleCommands[i]->_arguments[_simpleCommands[i]->_arguments.size()-1]->c_str());
    }

    //restore defaults and close file descriptors
    dup2(defaultIn, 0);
    dup2(defaultOut, 1);
    dup2(defaultErr, 2);

    close(defaultIn);
    close(defaultOut);
    close(defaultErr);

    int status = 0;
    if (!_background) { //for the parent process
      waitpid(pid, &status, 0);
      r = WEXITSTATUS(status);
    }
    else {
      exclaim = pid; //for last process run in the background
    }

    // Clear to prepare for next command
    clear();

    // Print new prompt
    Shell::prompt();
}

SimpleCommand * Command::_currentSimpleCommand;
