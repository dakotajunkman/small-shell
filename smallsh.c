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
int parseInput(char* command, struct command* commandStruct) {
    // empty out the arg array
    // https://stackoverflow.com/questions/58992845/what-is-the-standard-way-to-set-an-array-of-pointers-to-null
    for (size_t i = 0; i < MAX_ARGS; ++i)
        commandStruct->args[i] = NULL;

    int argIdx = 0;
    
    // set up some comparators for common inputs
    char* newline = "\n";
    char* comment = "#";
    char* reDirectOut = ">";
    char* redirectIn = "<";
    char* background = "&";
    char* pidVar = "$$";
    
    // dip early if the entry was blank or a comment
    if (command[0] == '\0' || command[0] == '#') return 0;
    

    // get the pid as a string
    pid_t pid = getpid();
    char pidStr[10];
    sprintf(pidStr, "%d", pid); // https://stackoverflow.com/questions/53230155/converting-pid-t-to-string

    char* saveptr;
    char* token;
    int argLength;
    
    // continually split the command on spaces FOREVER, jk only until it is NULL
    token = strtok_r(command, " ", &saveptr);
    while (token && argIdx < MAX_ARGS) {
        if (strcmp(token, pidVar) == 0) {
            argLength = strlen(pidStr) + 1;
            token = pidStr;
        } else
            argLength = strlen(token) + 1;
        commandStruct->args[argIdx] = (char*) malloc(argLength);
        if (argIdx == 0) {
            commandStruct->command = (char*) malloc(argLength);
            strcpy(commandStruct->command, token);
        }
        
        strcpy(commandStruct->args[argIdx++], token);
        token = strtok_r(NULL, " ", &saveptr);
    }

    return argIdx;
}

/**
 * Executes the cd command
 * Ref: https://stackoverflow.com/questions/1293660/is-there-any-way-to-change-directory-using-c-language
 */ 
void changeDirectory(char* dir, int argCount) {
    // when argCount is 1 no directory was given
    char* arg = argCount == 1 ? getenv("HOME") : dir;
    chdir(arg);
}

/**
 * Creates a new process to run a command
 * Ref: https://canvas.oregonstate.edu/courses/1890465/pages/exploration-process-api-executing-a-new-program?module_item_id=22511470
 */ 
void runProcess(struct command* command) {
    int childStatus;
    pid_t spawnPid = fork();

    switch (spawnPid) {
        // error
        case -1:
            perror("fork() failed!\n");
            exit(EXIT_FAILURE);
            break;
        
        // spawned process
        case 0:
            execvp(command->command, command->args);
            perror("execvp failed\n");
            exit(EXIT_FAILURE);
            break;

        // parent process
        default:
            spawnPid = waitpid(spawnPid, &childStatus, 0);
            break;
    }
}

/**
 * Attempts to execute the entered command
 */ 
void execCommand(struct command* command, int argCount) {
    char* cmd = command->command;
    if (strcmp(cmd, "cd") == 0)
        changeDirectory(command->args[1], argCount);
    else if (strcmp(cmd, "exit") == 0)
        return;
    else 
        runProcess(command);
}

/**
 * Frees memory allocated to store struct values
 */
void freeCommandStruct(struct command* command, int numArgs) {
    free(command->command);

    // Only want to free if memory was allocated
    // Ref: https://stackoverflow.com/questions/2688377/why-exactly-should-i-not-call-free-on-variables-not-allocated-by-malloc
    /*
    if (command->redirectTo[0] != '\0')
        free(command->redirectTo);

    if (command->redirectFrom[0] != '\0')
        free(command->redirectFrom);
    */

    for (int i = 0; i < numArgs; ++i)
        free(command->args[i]);
}

int main(void) {
    char command[MAX_INPUT];
    struct command commandStruct;
    commandStruct.runInBackground = false; // default value

    do {
        captureCommand(command);
        int commandArgs = parseInput(command, &commandStruct);
        if (commandArgs) {
            execCommand(&commandStruct, commandArgs);
            freeCommandStruct(&commandStruct, commandArgs);
        }
    } while (strcmp(command, "exit") != 0);

    return EXIT_SUCCESS;
}

