#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int frontPipe[2];
  pipe(frontPipe);

  // generate number list
  for(int i = 2; i <= 35; i++)
  {
    write(frontPipe[1], &i, sizeof(int));
  }
  int eof = -1;
  write(frontPipe[1], &eof, sizeof(int));
  close(frontPipe[1]);
  
  while(1)
  {
    int backPipe[2];
    pipe(backPipe);

    // pick up first number, must be prime.
    int prime;
    read(frontPipe[0], &prime, sizeof(int));
    if(prime == eof)
    {
      // clean up and die: no more number to deal with
      close(frontPipe[0]);
      close(backPipe[0]);
      close(backPipe[1]);
      break;
    }
    else
    { 
      int p = fork();

      if(p > 0) // parent
      {
        // clean up: won't read from backPipe and won't write to frontPipe
        close(backPipe[0]);
        close(frontPipe[1]);
        // the first number you get must be prime.
        printf("prime %d\n", prime);

        int feedBackPipe = 0;
        while(feedBackPipe != eof)
        {
          read(frontPipe[0], &feedBackPipe, sizeof(int));
          
          // feed the backPipe, and drop non-prime
          if(feedBackPipe % prime)
          {
            write(backPipe[1], &feedBackPipe, sizeof(int));
          }
        }
        // write complete, clean up and die
        close(backPipe[1]);
        break;
      }
      else if(p == 0) // child process
      {
        // clean up. frontPipe[1] was already closed before fork()
        close(frontPipe[0]);
        // back pipe is front pipe for child process now.
        frontPipe[0] = backPipe[0];
        frontPipe[1] = backPipe[1];
        // go looping: fork() and you are the parent now!
        continue;
      }
      else  // fork error
      {
        printf("Error: fork error");
        break;
      }
    }
  }
  exit();
}