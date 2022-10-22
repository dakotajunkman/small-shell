#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_INPUT 2048
#define MAX_ARGS 512
#define MAX_PROCESSES 100

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

/*
 * Processes structure
 * Contains info on currently running background processes
 * Started background processes will be kept here and checked once per cycle for completion
 * The array will be circular in nature
 * curIdx will be modded by MAX_ARGS to stay in bounds and write to the first 0 slot it finds
 */
struct processes {
    pid_t pids[MAX_PROCESSES];
    int curIdx; // where the next pid should be written in the array
};

// globals
int exitStatus = 0;
bool backGroundAllowed;

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

/*
 * Calculates number of bytes to malloc for arguments
 */
int calcMallocLength(char* token) {
    pid_t pid = getpid();
    char pidStr[10];
    sprintf(pidStr, "%d", pid); // https://stackoverflow.com/questions/53230155/converting-pid-t-to-string

    int pidLen = strlen(pidStr);
    int tokenLen = strlen(token);

    // iterate over the token and check for $$ instances
    for (size_t i = 0; i < strlen(token) - 1; ++i) {
        if (token[i] == '$' && token[i + 1] == '$') {
            tokenLen -= 2; // subtract out the length of "$$"
            tokenLen += pidLen; // add in length of pid
            i++; // we can skip the next character since we know it is $
        }
    }

    return tokenLen;
}

/**
 * Writes pid to argument string starting at passed in index
 */
void writePidToArg(char* pid, char* arg, int argIdx, int pidLen) {
    for (int i = 0; i < pidLen; ++i)
        arg[argIdx + i] = pid[i]; // offset argIdx by i to write forward in the array
}

/**
 * Copies token into arg
 * Expands $$ to pid
 */
void copyWithVarExpansion(char* token, char* arg) {
    pid_t pid = getpid();
    char pidStr[10];
    sprintf(pidStr, "%d", pid);

    int pidLen = strlen(pidStr);
    int tokenLen = strlen(token);

    int tokenIdx;
    int argIdx = 0;

    for (tokenIdx = 0; tokenIdx < tokenLen; ++tokenIdx) {
        // looking for instance of $$ in the token
        if (token[tokenIdx] == '$' && tokenIdx + 1 < tokenLen && token[tokenIdx + 1] == '$') {
            writePidToArg(pidStr, arg, argIdx, pidLen);
            argIdx += pidLen;
            tokenIdx++;
        } else {
            arg[argIdx++] = token[tokenIdx];
        }
    }
}

/*
 * Parses args array to populate input and output files
 * Searches for redirection symbols and sets appropriate source and destination filepaths
 * Removes the redirect symbol and file from args array
 */
void fillInRedirects(struct command* commandStruct) {
    int i = 0;

    while (commandStruct->args[i] != NULL) {
        char* arg = commandStruct->args[i];
        if (strcmp(arg, "<") == 0) {
            commandStruct->redirectFrom = malloc(strlen(commandStruct->args[i + 1]) + 1);
            strcpy(commandStruct->redirectFrom, commandStruct->args[i + 1]);

            // remove redirection from args, otherwise execvp() will try to use them
            commandStruct->args[i] = '\0';
            commandStruct->args[i + 1] = '\0';
            i += 2;
        } else if (strcmp(arg, ">") == 0) {
            commandStruct->redirectTo = malloc(strlen(commandStruct->args[i + 1]) + 1);
            strcpy(commandStruct->redirectTo, commandStruct->args[i + 1]);
            commandStruct->args[i] = '\0';
            commandStruct->args[i + 1] = '\0';
            i += 2;
        } else {
            i++;
        }
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
    
    // clean out redirection pointers
    commandStruct->redirectTo = NULL;
    commandStruct->redirectFrom = NULL;

    int argIdx = 0;
    
    // set up some comparators for common inputs
    char comment = '#';
    char nullterm = '\0';
    char* background = "&";
    
    // dip early if the entry was blank or a comment
    if (command[0] == nullterm || command[0] == comment) return 0;

    char* saveptr;
    char* token;
    int argLength;
    
    // continually split the command on spaces FOREVER, jk only until it is NULL lulz
    token = strtok_r(command, " ", &saveptr);
    while (token && argIdx < MAX_ARGS) {
        argLength = calcMallocLength(token) + 1; // + 1 for null terminator
        commandStruct->args[argIdx] = (char*) malloc(argLength);
        memset(commandStruct->args[argIdx], '\0', argLength);
        copyWithVarExpansion(token, commandStruct->args[argIdx]);
        if (argIdx == 0) {
            commandStruct->command = (char*) malloc(argLength);
            memset(commandStruct->command, '\0', argLength);
            strcpy(commandStruct->command, commandStruct->args[argIdx]);
        }
        argIdx++;
        token = strtok_r(NULL, " ", &saveptr);
    }
    
    // set background process flag
    commandStruct->runInBackground = (strcmp(commandStruct->args[argIdx - 1], background) == 0);

    // remove & from args if it is there so execvp does not scream
    if (commandStruct->runInBackground) {
        free(commandStruct->args[--argIdx]); // deallocate memory
        commandStruct->args[argIdx] = NULL; // set index to null and decrement argIdx since the argument is gone
        
        // set flag based on whether background commands are currently allowed
        // we can just set the flag to the value of backGroundAllowed
        commandStruct->runInBackground = backGroundAllowed;
    }

    // check for redirections
    fillInRedirects(commandStruct);

    return argIdx;
}

/**
 * Displays the exit status or terminating signal of the most recent foreground process.
 */
void displayStatus() {
    if (exitStatus == 0 || exitStatus == 1)
        printf("exit value %d\n", exitStatus);
    else
        printf("terminated by signal %d\n", exitStatus);

    fflush(stdout);
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

/*
 * Sets redirection input and output files if needed
 * Writes to stderr and exits upon error
 * Ref: https://canvas.oregonstate.edu/courses/1890465/pages/exploration-system-calls-and-reading-and-writing-files-in-c?module_item_id=22511436
 */
void setRedirect(struct command* command) {
    // create result and file descriptors
    int result;
    int sourceFd;
    int destFd;
    char* devNull = "/dev/null";

    if (command->redirectFrom) {
        sourceFd = open(command->redirectFrom, O_RDONLY);
        if (sourceFd == -1) {
            fprintf(stderr, "Cannot open file %s\n", command->redirectFrom);
            exit(1);
        }
        result = dup2(sourceFd, STDIN_FILENO);
        if (result == -1) {
            perror("Could not redirect stdin\n");
            exit(1);
        }
    } else {
        // redirection for background processes
        if (command->runInBackground) {
            sourceFd = open(devNull, O_RDONLY);
            if (sourceFd == -1) {
                perror("Cannot open /dev/null\n");
                exit(1);
            }

            result = dup2(sourceFd, STDIN_FILENO);
            if (result == -1) {
                perror("Could not redirect stdin\n");
                exit(1);
            }
        }
    } 
    if (command->redirectTo) {
        destFd = open(command->redirectTo, O_WRONLY | O_TRUNC | O_CREAT, 0644);
        if (destFd == -1) {
            fprintf(stderr, "Cannot open file %s\n", command->redirectTo);
            exit(1);
        }
        result = dup2(destFd, STDOUT_FILENO);
        if (result == -1) {
            perror("Could not redirect stdout\n");
            exit(1);
        }
    } else {
        // redirection for background processes
        if (command->runInBackground) {
            destFd = open(devNull, O_WRONLY | O_TRUNC | O_CREAT, 0644);
            if (destFd == -1) {
                perror("Could not open /dev/null\n");
                exit(1);
            }
            result = dup2(destFd, STDOUT_FILENO);
            if (result == -1) {
                perror("Could not redirect stdout\n");
                exit(1);
            }
        }
    }
}

/*
 * Writes the pid to the first 0 slot it finds
 * Uses modulo to create a circular traversal of the array
 */
void writePidToProcessArray(struct processes* processes, pid_t pid) {
    int idx = processes->curIdx;

    // traverse until we hit a 0 slot
    while (processes->pids[idx] != 0) 
        idx = (idx + 1) % MAX_ARGS;

    // write in the pid and update curIdx to the new slot
    processes->pids[idx] = pid;
    processes->curIdx = ++idx;
}

/**
 * Sets up child process signal handling
 */
void createChildSigHandlers(struct command* command) {
    // foreground process needs a different SIGINT handler
    // Ref: https://stackoverflow.com/questions/49404021/different-signal-handlers-for-parent-and-child
    if (!command->runInBackground)
        signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_IGN);
}

/**
 * Creates a new process to run a command
 * Ref: https://canvas.oregonstate.edu/courses/1890465/pages/exploration-process-api-executing-a-new-program?module_item_id=22511470
 */ 
void runProcess(struct command* command, struct processes* processes) {
    int childStatus;
    pid_t spawnPid = fork();

    switch (spawnPid) {
        // error
        case -1:
            perror("fork() failed!\n");
            exit(1);
            break;
        
        // spawned process
        case 0:
            createChildSigHandlers(command);
            setRedirect(command);
            execvp(command->command, command->args);
            perror("execvp failed");
            exit(1);
            break;

        // parent process
        default:
            // when background process, we just need to record the pid and move on
            if (command->runInBackground) {
                writePidToProcessArray(processes, spawnPid);
                printf("background pid is %d\n", spawnPid);
                fflush(stdout);
            } else {
                spawnPid = waitpid(spawnPid, &childStatus, 0);

                // find out if process exited properly
                // Ref: https://canvas.oregonstate.edu/courses/1890465/pages/exploration-process-api-monitoring-child-processes?module_item_id=22511469
                if (WIFEXITED(childStatus))
                    exitStatus = WEXITSTATUS(childStatus);
                else
                    exitStatus = WTERMSIG(childStatus);

                // if process was killed by SIGINT we need to print it immediately
                if (exitStatus == 2)
                    displayStatus();
            }
            break;
    }
}

/**
 * Attempts to execute the entered command
 */ 
void execCommand(struct command* command, struct processes* processes, int argCount) {
    char* cmd = command->command;
    if (strcmp(cmd, "cd") == 0)
        changeDirectory(command->args[1], argCount);
    else if (strcmp(cmd, "exit") == 0)
        return;
    else if (strcmp(cmd, "status") == 0)
        displayStatus();
    else 
        runProcess(command, processes);
}

/**
 * Frees memory allocated to store struct values
 */
void freeCommandStruct(struct command* command, int numArgs) {
    free(command->command);

    // Only want to free if memory was allocated
    // Ref: https://stackoverflow.com/questions/2688377/why-exactly-should-i-not-call-free-on-variables-not-allocated-by-malloc
    if (command->redirectTo)
        free(command->redirectTo);

    if (command->redirectFrom)
        free(command->redirectFrom);

    for (int i = 0; i < numArgs; ++i)
        free(command->args[i]);
}

/*
 * Checks currently running background processes for completion
 * Prints out status of completed processes
 * Ref: https://canvas.oregonstate.edu/courses/1890465/pages/exploration-process-api-monitoring-child-processes?module_item_id=22511469
 */
void checkBackgroundProcesses(struct processes* processes) {
    // setup variables that we will need
    int result;
    int status;

    for (int i = 0; i < MAX_PROCESSES; ++i) {
        pid_t pid = processes->pids[i];

        // we need to check it for completion
        if (pid != 0) {
            result = waitpid(pid, &status, WNOHANG);
            
            // process has completed, we need to display the status
            if (result != 0) {
                
                // exited normally
                if (WIFEXITED(status))
                    printf("background pid %d is done: exit value %d\n", pid, WEXITSTATUS(status));
                else
                    printf("background pid %d is done: terminated by signal %d\n", pid, WTERMSIG(status));

                fflush(stdout);
                processes->pids[i] = 0; // frees up space to be used again now that process is done
            }
        }
    }
}

/**
 * Handles SIGTSTP
 * Toggles whether background processes are allowed
 * Notifies user of entering and exiting foreground only mode
 */
void handleSIGSTP() {
    if (backGroundAllowed) 
        write(STDOUT_FILENO, "Entering foreground-only mode (& is now ignored)\n", 49);
    else
        write (STDOUT_FILENO, "Exiting foreground-only mode\n", 29);

    // reprompt for input
    write(STDOUT_FILENO, ": ", 2);

    backGroundAllowed = !backGroundAllowed;
}

/**
 * Creates signal handlers to properly handle SIGINT and SIGTSTP
 * Ref: https://canvas.oregonstate.edu/courses/1890465/pages/exploration-signal-handling-api?module_item_id=22511478
 */
void createSigHandlers() {
    struct sigaction sigintHandler;
    sigintHandler.sa_handler = SIG_IGN;
    sigfillset(&sigintHandler.sa_mask);
    sigintHandler.sa_flags = 0;
    sigaction(SIGINT, &sigintHandler, NULL);

    struct sigaction sigtstpHandler;
    sigtstpHandler.sa_handler = handleSIGSTP;
    sigfillset(&sigtstpHandler.sa_mask);
    sigtstpHandler.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sigtstpHandler, NULL);
}

/**
 * Loops over processes array and kills any outstanding processes
 */
void killBackgroundProcesses(struct processes* processes) {
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        if (processes->pids[i] != 0)
            kill(processes->pids[i], SIGKILL);
    }
}

int main(void) {
    char command[MAX_INPUT];
    struct command commandStruct;
    struct processes processes;
    
    // clean out the pid memory space
    for (int i = 0; i < MAX_PROCESSES; ++i)
        processes.pids[i] = 0;

    processes.curIdx = 0;
    backGroundAllowed = true; // default here to start

    // set up the custom signal handlers
    createSigHandlers();

    do {
        captureCommand(command);
        int commandArgs = parseInput(command, &commandStruct);
        if (commandArgs) {
            execCommand(&commandStruct, &processes, commandArgs);
            freeCommandStruct(&commandStruct, commandArgs);
        }
        checkBackgroundProcesses(&processes);
    } while (strcmp(command, "exit") != 0);

    killBackgroundProcesses(&processes);
    return EXIT_SUCCESS;
}

