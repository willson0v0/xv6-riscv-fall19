#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  // int i;
  if(argc != 2)
  {
    printf("Error: There must be exactly one argument.\n");
  } 
  else 
  {
    int length = 0;
    for(int i = 0; i < strlen(argv[1]); i++)
    {
      if(argv[1][i] >= '0' && argv[1][i] <= '9') {
        length *= 10;
        length += argv[1][i] - '0';
      }
      else
      {
        printf("Error: Invalid character \'%c\'. Argument must be integer.\n", argv[1][i]);
        exit();
      }
    }
    printf("Sleep %d\n", length);
    sleep(length);
  }
  exit();
}
