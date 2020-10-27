#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define XARG_PARAM_MAX_LENGTH 128

int
main(int argc, char* argv[])
{
  char** buf;
  buf = (char**)malloc(sizeof(char*) * MAXARG);
  if(argc == 1)
  {
    printf("Error: xargs required at least one argument.");
  }
  else
  {
    while(1)
    {
      for(int j = 1; j < argc; j++)
      {
        buf[j-1] = (char*)malloc(sizeof(char) * XARG_PARAM_MAX_LENGTH);
        strcpy(buf[j-1], argv[j]);
      }
      for(int j = argc; j < MAXARG; j++)
      {
        if(buf[j])
        {
          free(buf[j]);
          buf[j] = 0;
        }
      }
      int i = argc - 1;
      buf[i] = (char*)malloc(sizeof(char) * XARG_PARAM_MAX_LENGTH);
      char* p = buf[i];
      while(1)
      {
        char c;
        int readRes = read(0, &c, sizeof(char));
        if(readRes == 0) exit(); // exit on EOF?
        
        if(c == ' ')  // split input.
        {
          i++;
          p = (char*)malloc(sizeof(char) * XARG_PARAM_MAX_LENGTH);
          buf[i] = p;
          
          if(i == MAXARG)
          {
            printf("Error: too many arguments.\n");
            exit();
          }
        }
        else if(c == '\n')
        {
          int childPid = fork();

          if(childPid < 0)
          {
            printf("Eror: fork error\n");
          }
          else if (childPid == 0)
          {
            exec(argv[1], buf);
            exit();
          }

          wait();
          break;
        }
        else if(c == '\0')
        {
          exit(); // Exit on EOF?
        }
        else
        {
          *p = c;
          p++;
          if((p - buf[i]) >= XARG_PARAM_MAX_LENGTH)
          {
            printf("Error: Argmuent too long\n");
            exit();
          }
        }
      }
    }
  }

  // clean up and die
  for(int i = 0; i < MAXARG; i++)
  {
    if(buf[i])
      free(buf[i]);
  }

  free(buf);

  exit();
}