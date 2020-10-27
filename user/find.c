#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


// Regexp matcher from Kernighan & Pike,
// The Practice of Programming, Chapter 9.

int matchhere(char*, char*);
int matchstar(int, char*, char*);

int
match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{  // must look at empty string
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
  do{  // a * matches zero or more instances
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}

void
dfs(char* path, char* pattern)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  
  if((fd = open(path, 0)) < 0) 
  {
    return;
  }

  if(fstat(fd, &st) < 0)
  {
    close(fd);
    return;
  }

  if(match(pattern, path))
  {
    printf("%s\n", path);
  }

  switch (st.type)
  {
  case T_FILE:
    close(fd);
    break;
  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(strcmp(de.name, ".") && strcmp(de.name, ".."))
      {
        dfs(buf, pattern);
      }
    }
    close(fd);
    break;
  
  default:
    break;
  }
}

int
main(int argc, char* argv[])
{
  if(argc == 2)
  {
    dfs(".", argv[1]);
  }
  else if(argc > 2)
  {
    for(int i = 2; i < argc; i++)
    {
      dfs(argv[1], argv[i]);
    }
  }
  else
  {
    printf("Error: too less argments, accept at least 1.");
  }

  exit();
}