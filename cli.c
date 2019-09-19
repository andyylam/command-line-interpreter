#include <stdio.h>
#include <fcntl.h> //For stat()
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h> //for waitpid()
#include <unistd.h>   //for fork(), wait()
#include <string.h>   //for string comparison etc
#include <stdlib.h>   //for malloc()

#define READ_END 0
#define WRITE_END 1

char **readTokens(int maxTokenNum, int maxTokenSize, int *readTokenNum, char *buffer);
void freeTokenArray(char **strArr, int size);
int checkValidPaths(char *token);
int checkValidCommandPath(char *token);
void executeCommand(char **tokenStrArr);
void setUpCommands(char **tokenStrArr, int pipeIndices[], int tokenNum, int pipeCount);
void executePipe(char **command1, char **command2);
int getPipeIndices(char **tokenStrArr, int pipeIndices[], int tokenNum);
void getTokenArr(char **tokenStrArr, char **command, int start, int end);
void executeCommands(char **commands[], int pipeCount);
void freeCommands(char **commands[], int size);

char *DEFAULT_PATH = "/bin";
char *NOT_FOUND = "not found";

int main()
{
    char command[200];
    int tokenNum;
    int i;
    char **tokenStrArr;
    while (1)
    {
        // Read one line from stdin and stores in command
        fgets(command, 200, stdin);
        if (command[0] == '\n' || command[0] == '\0')
        {
            continue;
        }
        // Read tokens from command and stores in tokenStrArr
        tokenStrArr = readTokens(10, 19, &tokenNum, command);
        // Check termination condition
        if (strncmp(tokenStrArr[0], "quit", 4) == 0)
        {
            freeTokenArray(tokenStrArr, tokenNum);
            tokenStrArr = NULL;
            break;
        }
        // Record indices for pipe (maximum 10 chained commands)
        int pipeIndices[10];
        int pipeCount = getPipeIndices(tokenStrArr, pipeIndices, tokenNum);
        if (pipeCount == 0)
        {
            // CASE 1: if no pipe, execute normal command
            if (checkValidPaths(tokenStrArr[0]))
            {
                // CASE 1A: valid command path
                if (fork() == 0)
                {
                    executeCommand(tokenStrArr);
                }
                else
                {
                    wait(NULL);
                }
            }
            else if (strncmp(tokenStrArr[0], "set", 3) == 0 && tokenNum == 3)
            {
                // CASE 1B: set command
                if (setenv(tokenStrArr[1], tokenStrArr[2], 1) != 0)
                {
                    perror("set");
                    exit(EXIT_FAILURE);
                }
            }
            else if (strncmp(tokenStrArr[0], "unset", 5) == 0 && tokenNum == 2)
            {
                // CASE 1C: unset command
                if (unsetenv(tokenStrArr[1] + 1) != 0)
                {
                    perror("unset");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                // CASE 1D: no command path found, not env command
                printf("%s %s\n", tokenStrArr[0], NOT_FOUND);
            }
        }
        else
        {
            // CASE 2: if pipe exists, execute pipe commands
            setUpCommands(tokenStrArr, pipeIndices, tokenNum, pipeCount);
        }
        freeTokenArray(tokenStrArr, tokenNum);
        tokenStrArr = NULL;
    }
    printf("Goodbye!\n");
    return 0;
}

int getPipeIndices(char **tokenStrArr, int pipeIndices[], int tokenNum)
{
    // Store indices where string "|" appears into pipeIndices[] and count number of pipes
    int i;
    int count = 0;
    for (i = 0; i < tokenNum; i++)
    {
        if (strcmp(tokenStrArr[i], "|") == 0)
        {
            pipeIndices[count++] = i;
        }
    }
    return count;
}

void setUpCommands(char **tokenStrArr, int pipeIndices[], int tokenNum, int pipeCount)
{
    // Initialise **commands[], an array of commands
    // Memcpy from tokenStrArr to commands, where commands are separated by "|"
    // This is done with the pipeIndices[]
    int start = 0;
    int end = pipeIndices[0];
    int i;
    int j;
    char **commands[pipeCount + 1];
    for (int i = 0; i < pipeCount + 1; i++)
    {
        commands[i] = (char **)malloc(sizeof(char *) * 19);
    }
    getTokenArr(tokenStrArr, commands[0], start, end);
    for (i = 1; i < pipeCount; i++)
    {
        start = end + 1;
        end = pipeIndices[i];
        getTokenArr(tokenStrArr, commands[i], start, end);
    }
    getTokenArr(tokenStrArr, commands[pipeCount], end + 1, tokenNum);
    for (i = 0; i < pipeCount + 1; i++)
    {
        // Check all commands for validity
        if (!checkValidPaths(commands[i][0]))
        {
            printf("%s %s\n", commands[i][0], NOT_FOUND);
            freeCommands(commands, pipeCount + 1);
            return;
        }
    }
    // Execute pipe commands
    executeCommands(commands, pipeCount);
    // Free **commands[] memory
    freeCommands(commands, pipeCount + 1);
}

void freeCommands(char **commands[], int size)
{
    // Frees commands array
    int i;
    for (i = 0; i < size; i++)
    {
        free(commands[i]);
        commands[i] = NULL;
    }
}

void executeCommands(char **commands[], int pipeCount)
{
    // Execute pipe commands
    int pipes[pipeCount][2]; // Create an array of pipes
    int i;
    int pid;
    for (i = 0; i < pipeCount; i++)
    {
        // Terminate process if error occurs in creating pipes
        if (pipe(pipes[i]) == -1)
        {
            printf("Could not create the pipe\n");
            exit(EXIT_FAILURE);
        }
    }
    for (i = 0; i < pipeCount + 1; i++)
    {
        // Fork, and child process executes command
        if ((pid = fork()) == 0)
        {
            int j;
            for (j = 0; j < pipeCount; j++)
            {
                if (j == i - 1)
                {
                    // Pipe of index (command - 1) is where the child process reads from
                    if (dup2(pipes[j][READ_END], STDIN_FILENO) < 0)
                    {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                }
                if (j == i)
                {
                    // Pipe of index command is where the child process writes to
                    if (dup2(pipes[j][WRITE_END], STDOUT_FILENO) < 0)
                    {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                }
                // Close all pipes
                close(pipes[j][READ_END]);
                close(pipes[j][WRITE_END]);
            }
            // Execute the command
            executeCommand(commands[i]);
            exit(1);
        }
    }
    int status;
    for (int j = 0; j < pipeCount + 1; j++)
    {
        // Parent process waits for all child processes and closes all pipes
        wait(&status);
        if (j != pipeCount)
        {
            close(pipes[j][READ_END]);
            close(pipes[j][WRITE_END]);
        }
        if (status == -1)
        {
            printf("Error in child\n");
        }
    }
}

// returns 1 if specified command path OR default path is valid, else returns 0.
int checkValidPaths(char *token)
{
    char path[200];
    // check if command path is valid
    int commandPath = checkValidCommandPath(token);
    // if not valid, check default command path
    if (!commandPath)
    {
        sprintf(path, "%s/%s", DEFAULT_PATH, token);
        int defaultPath = checkValidCommandPath(path);
        if (!defaultPath)
        {
            return 0;
        }
    }
    return 1;
}

// Check specified command path
int checkValidCommandPath(char *token)
{
    struct stat buf;
    int res = stat(token, &buf);
    if (res == 0)
    {
        return 1;
    }
    return 0;
}

// Copies from tokenStrArr between indices start (inclusive) and end (exclusive) to command
// Sets last to NULL to facilitate use of execv
void getTokenArr(char **tokenStrArr, char **command, int start, int end)
{
    memcpy(command, tokenStrArr + start, (end - start) * sizeof(char *));
    command[end - start] = NULL;
}

void executeCommand(char **tokenStrArr)
{
    int i;
    if (strstr(tokenStrArr[0], "bin") == NULL)
    {
        char path[200];
        sprintf(path, "%s/%s", DEFAULT_PATH, tokenStrArr[0]);

        // Convert all env variables (prefixed with $) to its set variables
        // Maximum size is 10
        // Break loop if end of array (NULL) is reached
        for (i = 0; i < 10; i++)
        {
            if (tokenStrArr[i] == NULL)
            {
                break;
            }
            if (strncmp(tokenStrArr[i], "$", 1) == 0)
            {
                tokenStrArr[i] = getenv(tokenStrArr[i] + 1);
            }
        }
        execv(path, &tokenStrArr[0]);
    }
    execv(tokenStrArr[0], &tokenStrArr[0]);
}

char **readTokens(int maxTokenNum, int maxTokenSize, int *readTokenNum, char *buffer)
{
    //Tokenize buffer
    //Assumptions:
    //  - the tokens are separated by " " and ended by newline
    //Return: Tokenized substrings as array of strings
    //        readTokenNum stores the total number of tokens
    //Note:
    //  - should use the freeTokenArray to free memory after use!
    char **tokenStrArr; // pointer of char pointers -> array of strings
    char *token;        // pointer of chars -> string
    int i;

    //allocate token array, each element is a char*
    tokenStrArr = (char **)malloc(sizeof(char *) * maxTokenNum);

    //Nullify all entries
    for (i = 0; i < maxTokenNum; i++)
    {
        tokenStrArr[i] = NULL;
    }
    // strtok breaks string (buffer) into tokens with given delimiter
    token = strtok(buffer, " \n");

    i = 0;
    while (i < maxTokenNum && (token != NULL))
    {
        //Allocate space for token string
        tokenStrArr[i] = (char *)malloc(sizeof(char *) * maxTokenSize);

        //Ensure at most 19 + null characters are copied
        strncpy(tokenStrArr[i], token, maxTokenSize - 1);

        //Add NULL terminator in the worst case
        tokenStrArr[i][maxTokenSize - 1] = '\0';

        i++;
        //To signal to keep searching the same string, pass a NULL pointer as first argument
        token = strtok(NULL, " \n");
    }

    *readTokenNum = i;

    return tokenStrArr;
}

void freeTokenArray(char **tokenStrArr, int size)
{
    int i = 0;

    //Free every string stored in tokenStrArr
    for (i = 0; i < size; i++)
    {
        if (tokenStrArr[i] != NULL)
        {
            free(tokenStrArr[i]);
            tokenStrArr[i] = NULL;
        }
    }
    //Free entire tokenStrArr
    free(tokenStrArr);

    //Note: Caller still need to set the strArr parameter to NULL
    //      afterwards
}
