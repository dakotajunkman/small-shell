#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#define MAX_INPUT 2048
#define MAX_ARGS 512

/*
 * Command structure
 * Represents a command input by the user
 * Command will be parsed to fill in the attributes
 */
struct command {
    char* command;
    char* args[MAX_ARGS];
    char* redirectFrom;
    char* redirectTo;
    bool runInBackground;
};

/**
 * Captures input from terminal and places it into inputArr
 * Ref: https://stackoverflow.com/questions/45931613/using-getchar-inside-an-infinite-loop-in-c
 */ 
void captureCommand(char* inputArr) {
   int idx = 0;
   int inputChar;
   int newline = '\n';
   int nullterm = '\0';
    
    // prompt user for input
    printf(": ");
    fflush(stdout);
    
    // gather input one character at a time until newline is hit
   for (;;) {
       inputChar = getchar();

       // break out when newline is hit or input reaches max
       if (inputChar == newline || idx == MAX_INPUT - 1) break;

       inputArr[idx++] = inputChar;
   }

   // terminate this thing
   inputArr[idx] = nullterm;
}

int main(void) {
    char command[MAX_INPUT];

    do {
        captureCommand(command);
        printf("command was: %s\n", command);
    } while (strcmp(command, "exit") != 0);

    return EXIT_SUCCESS;
}

