#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

#define MAX_COMMAND_LENGTH  512
#define MAX_ARG_COUNT       16
#define MAX_ARG_LENGTH      16
#define MAX_PATH_LENGTH     512
#define BUFFER_SIZE         512

char* FORK_ERR = "Error: Fork error";
char* READ_ERR = "Error: Read failed";
char* CD_ERR = "Error: Cannot cd to %s";

// Basic structure
// command [args] [< input_redir] [(> output_redir) \or (| command [args] [> output_redir])]

struct MemberCmd
{
    char exceName[MAX_COMMAND_LENGTH];          // Full path
    char args[MAX_ARG_COUNT][MAX_ARG_LENGTH];
    // char inputRedirFile[MAX_PATH_LENGTH];
    int inputRedirFD;
};

struct Cmd
{
    struct MemberCmd pipeSource;
    struct MemberCmd pipeSink;
    // char   outputRedirFile[MAX_PATH_LENGTH];
    int sourceOutputRedirFD;
};

int
safeFork(void)
{
    int pid = fork();
    if(pid < 0)
        fprintf(2, "%s\n", FORK_ERR);
    return pid;
}

struct Cmd
parseInput(char* input)
{

}

void
execCmd(struct Cmd cmd)
{

}

int
readFromInput(char* buffer)
{
    fprintf(2, "@");
    memset(buffer, 0, BUFFER_SIZE);
    return read(0, buffer, BUFFER_SIZE);
}

int main(int argc, char* argv[])
{
    char buf[BUFFER_SIZE];
    while (readFromInput(buf))
    {
        if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ')
        {
            buf[strlen(buf) - 1] = 0;
            if(chdir(buf+3) < 0)
                fprintf(2, CD_ERR, buf+3);
            continue;
        }
        if(!safeFork())
            execCmd(parseInput(buf));
        wait(0);
    }
    exit(0);
}
