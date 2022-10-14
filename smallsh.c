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

