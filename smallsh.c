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
   
    // cleans out whatever was in the array previously
    memset(inputArr, nullterm, MAX_INPUT);

    printf(": ");
    fflush(stdout);
    
    for (;;) {
        inputChar = getchar();
        if (inputChar == newline || idx == MAX_INPUT - 1) break;
        inputArr[idx++] = inputChar;
   }
}

/**
 * parses command input by the user and fills in the command struct
 */ 
bool parseInput(char* command, struct command* commandStruct) {
    // empty out the arg array
    memset(commandStruct->args, 0, MAX_ARGS);
    int argIdx = 0;
    
    // set up some comparators for common inputs
    char* newline = "\n";
    char* comment = "#";
    char* reDirectOut = ">";
    char* redirectIn = "<";
    char* background = "&";
    char* pidVar = "$$";
    
    // dip early if the entry was blank or a comment
    if (command[0] == '\0' || command[0] == '#') return false;

    return true;
}

int main(void) {
    char command[MAX_INPUT];
    struct command commandStruct;

    do {
        captureCommand(command);
    } while (strcmp(command, "exit") != 0);

    return EXIT_SUCCESS;
}

