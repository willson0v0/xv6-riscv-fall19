#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int parent2child[2];
  int child2parent[2];
  pipe(parent2child);
  pipe(child2parent);

  char buffer[100];

  int f = fork();

  char* pingpong[2] = {"ping", "pong"};
  
  if(f < 0) 
  {
    printf("Error: Fork error.");
  }
  else if(f == 0)
  {
    close(parent2child[1]);
    close(child2parent[0]);

    read(parent2child[0], buffer, 5);
    printf("%d: received %s\n", getpid(), buffer);
    write(child2parent[1], pingpong[1], strlen(pingpong[1]) + 1);
  }
  else 
  {
    close(parent2child[0]);
    close(child2parent[1]);

    write(parent2child[1], pingpong[0], strlen(pingpong[0]) + 1);
    read(child2parent[0], buffer, 5);
    printf("%d: received %s\n", getpid(), buffer);
  }
  exit();
}
