#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

#define MAX_COMMAND_LENGTH 128
#define MAX_ARG_COUNT 8
#define MAX_ARG_LENGTH 16
#define MAX_PATH_LENGTH 128
#define BUFFER_SIZE 128

#define NONPIPE 0
#define PIPE 1

char *FORK_ERR = "Error: Fork error\n";
char *READ_ERR = "Error: Read failed\n";
char *CD_ERR = "Error: Cannot cd to %s\n";
char *PARSE_ERR = "Error: Cannot parse your input.\ncmd structure: command [args] [< input_redir] [(> output_redir) \\OR (| command [args] [> output_redir])]\n";

// Basic structure
// command [args] [< input_redir] [(> output_redir) \or (| command [args] [> output_redir])]

struct MemberCmd
{
    char exceName[MAX_COMMAND_LENGTH]; // Full path
    char args[MAX_ARG_COUNT][MAX_ARG_LENGTH];
    // char inputRedirFile[MAX_PATH_LENGTH];
    char inputRedir[BUFFER_SIZE];
    char outputRedir[BUFFER_SIZE];
};

struct Cmd
{
    struct MemberCmd pipeSource;
    struct MemberCmd pipeSink;
    // char   outputRedirFile[MAX_PATH_LENGTH];
    int pipeType;
};

int find(char *str, char chr)
{
    int i = 0;
    while (*str)
    {
        if (*str == chr)
            return i;
        str++;
        i++;
    }
    return -1;
}

int safeFork(void)
{
    int pid = fork();
    if (pid < 0)
        fprintf(2, "%s\n", FORK_ERR);
    return pid;
}

void strip(char *input)
{
    char *stop = input + strlen(input) - 1;
    while (*stop == ' ' || *stop == '\n')
    {
        *stop = '\0';
        stop--;
    }
    char *start = input;
    while (*start == ' ' || *start == '\n')
        start++;
    strcpy(input, start);
}

int parseMemberCmd(char *input, struct MemberCmd *mCmd)
{
    strip(input);
    int inputRedir = find(input, '<');
    int outputRedir = find(input, '>');
    if (inputRedir != -1)
    {
        if (outputRedir != -1)
            input[outputRedir] = '\0';
        strcpy(mCmd->inputRedir, input + inputRedir + 1);
        if (outputRedir != -1)
            input[outputRedir] = '>';
        strip(mCmd->inputRedir);
    }
    else
    {
        strcpy(mCmd->inputRedir, "console");
    }

    if (outputRedir != -1)
    {
        if (inputRedir != -1)
            input[inputRedir] = '\0';
        strcpy(mCmd->outputRedir, input + outputRedir + 1);
        if (inputRedir != -1)
            input[inputRedir] = '>';
        strip(mCmd->outputRedir);
    }
    else
    {
        strcpy(mCmd->outputRedir, "console");
    }

    input[inputRedir] = '\0';
    input[outputRedir] = '\0';

    char *p1 = mCmd->args[0];
    char *p2 = input;
    char *p3 = mCmd->exceName;
    while (*p2 != ' ' && *p2 != '\0')
    {
        if (*p2 == '/')
        {
            p1 = mCmd->args[0];
        }
        else
        {
            *p1 = *p2;
            p1++;
        }
        *(p3++) = *(p2++);
    }
    *(p1 + 1) = '\0';
    *(p3 + 1) = '\0';

    int i = 1;
    char buf[BUFFER_SIZE];
    char *p = buf;
    strcpy(p, p2);
    strip(p);
    while (*p)
    {
        int div = find(p, ' ');
        if (div != -1)
            p[div] = '\0';
        strcpy(mCmd->args[i++], p);
        if (div != -1)
            p[div] = ' ';
        else
            break;
        p = p + div + 1;
        strip(p);
    }
    mCmd->args[i][0] = '\0';

    // fprintf(2, "==================== result for sub-command %s ====================\n", input);
    // fprintf(2, "execName:\t%s\n", mCmd->exceName);
    // for(int i = 0; mCmd->args[i][0]; i++)
    // {
    //     fprintf(2, "arg[%d]:\t\t%s\n", i, mCmd->args[i]);
    // }
    // fprintf(2, "inRedir:\t%s\n", mCmd->inputRedir);
    // fprintf(2, "outRedir:\t%s\n", mCmd->outputRedir);
    return 1;
}

int parseInput(char *input, struct Cmd *cmd)
{
    int i = find(input, '|');
    char leftCmd[BUFFER_SIZE];
    char rightCmd[BUFFER_SIZE];
    if (i == -1)
    {
        strcpy(leftCmd, input);
        parseMemberCmd(leftCmd, &(cmd->pipeSource));

        cmd->pipeType = NONPIPE;
    }
    else
    {
        input[i] = '\0';
        strcpy(leftCmd, input);
        input[i] = '|';
        strcpy(rightCmd, input + i + 1);
        parseMemberCmd(leftCmd, &(cmd->pipeSource));
        parseMemberCmd(rightCmd, &(cmd->pipeSink));

        cmd->pipeType = PIPE;
    }
    return 1;
}

void execFromArray(char *execName, char args[MAX_ARG_COUNT][MAX_ARG_LENGTH])
{
    char *argv[MAX_ARG_COUNT];
    int i;
    for (i = 0; args[i][0]; i++)
    {
        argv[i] = args[i];
    }
    argv[i] = 0;
    exec(execName, argv);
    fprintf(2, "nsh exec %s failed\n", execName);
}

void execCmd(struct Cmd cmd)
{
    if (cmd.pipeType == NONPIPE)
    {
        int p = safeFork();
        if (p > 0)
        {
            wait(0);
        }
        else
        {
            if (strcmp(cmd.pipeSource.inputRedir, "console"))
            {
                close(0);
                open(cmd.pipeSource.inputRedir, O_RDONLY);
            }

            if (strcmp(cmd.pipeSource.outputRedir, "console"))
            {
                close(1);
                open(cmd.pipeSource.outputRedir, O_WRONLY | O_CREATE);
            }

            execFromArray(cmd.pipeSource.exceName, cmd.pipeSource.args);
        }
    }
    else if (cmd.pipeType == PIPE)
    {
        int cmdPipe[2];
        pipe(cmdPipe);

        if (fork() == 0)
        {
            if (strcmp(cmd.pipeSource.inputRedir, "console"))
            {
                close(0);
                open(cmd.pipeSource.inputRedir, O_RDONLY);
            }

            close(1);
            dup(cmdPipe[1]);
            close(cmdPipe[0]);
            close(cmdPipe[1]);

            execFromArray(cmd.pipeSource.exceName, cmd.pipeSource.args);
        }
        if (fork() == 0)
        {
            if (strcmp(cmd.pipeSink.outputRedir, "console"))
            {
                close(1);
                open(cmd.pipeSink.outputRedir, O_WRONLY | O_CREATE);
            }

            close(0);
            dup(cmdPipe[0]);
            close(cmdPipe[0]);
            close(cmdPipe[1]);
            execFromArray(cmd.pipeSink.exceName, cmd.pipeSink.args);
        }
    
        close(cmdPipe[0]);
        close(cmdPipe[1]);

        wait(0);
        wait(0);
    }
}

int readFromInput(char *buffer)
{
    // fprintf(2, "\r                          \r@ ");
    fprintf(2, "@ ");
    memset(buffer, 0, BUFFER_SIZE);
    gets(buffer, BUFFER_SIZE);
    strip(buffer);
    return buffer[0] == 0 ? -1 : 0;
}

int main(int argc, char *argv[])
{
    char buf[BUFFER_SIZE];
    while (readFromInput(buf) == 0)
    {
        if (strlen(buf) == 0)
            continue;
        if (strlen(buf) >= 3 && buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ')
        {
            buf[strlen(buf) - 1] = 0;
            if (chdir(buf + 3) < 0)
                fprintf(2, CD_ERR, buf + 3);
            continue;
        }
        struct Cmd parsed;
        memset(&parsed, 0, sizeof(parsed));
        if (!parseInput(buf, &parsed))
        {
            fprintf(2, PARSE_ERR);
        }
        else if (!safeFork())
        {
            execCmd(parsed);
        }
        else
        {
            wait(0);
        }
    }
    exit(0);
}
