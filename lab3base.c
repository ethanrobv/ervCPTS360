/***** LAB3 base code *****/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define PATH_SIZE 128
#define ARG_SIZE 64

char gpath[PATH_SIZE]; // hold token strings
char *arg[ARG_SIZE];   // token string pointers
int n;                 // number of token strings

char dpath[PATH_SIZE]; // hold dir strings in PATH
char *dir[ARG_SIZE];   // dir string pointers
int ndir;              // number of dirs

char *argH[ARG_SIZE], *argT[ARG_SIZE];
char headPath[PATH_SIZE], tailPath[PATH_SIZE];

int tokenize(char *pathname) // YOU have done this in LAB2
{                            // YOU better know how to apply it from now on
  char *s;
  strcpy(gpath, pathname); // copy into global gpath[]
  s = strtok(gpath, " ");
  n = 0;
  while (s)
  {
    arg[n++] = s; // token string pointers
    s = strtok(0, " ");
  }
  arg[n] = 0; // arg[n] = NULL pointer
}

int read_path(char **env)
{
  ndir = 0;
  for (int i = 0; env[i] != NULL; i++)
  {
    //parse *env[] for PATH
    char *temp1 = strtok(env[i], "=");
    if (!strcmp(temp1, "PATH"))
    {
      char *temp2 = strtok(NULL, ":");
      for (int j = 0; temp2 != NULL; j++)
      {
        dir[j] = temp2;
        ndir++;
        temp2 = strtok(NULL, ":");
      }
    }
  }
  //add cwd to dir list
  char *t = "./";
  dir[ndir] = t;
  ndir++;

  return 0;
}

//splits arg list into head and tail around a pipe symbol
//if no pipe symbol, head == args
int split_args(char **args)
{
  int flag = 0;
  for (int i = 0; args[i] != NULL; i++)
  {
    if (!strcmp(args[i], "|"))
      flag = i;
  }

  //no pipes
  if (!flag)
  {
    int j;
    for (j = 0; args[j] != NULL; j++)
    {
      argH[j] = args[j];
    }
    argH[j] = NULL;
    argT[0] = NULL;
    return 1;
  }

  //yes pipes
  int hn = 0, tn = 0;
  for (int j = 0; j < n; j++)
  {
    if (j < flag)
    {
      argH[hn] = args[j];
      hn++;
    }
    else if (j > flag)
    {
      argT[tn] = args[j];
      tn++;
    }
  }
  argT[tn] = NULL;
  argH[hn] = NULL;

  return 0;
}

int IO_redirect(char **args)
{
  for (int i = 0; args[i] != NULL; i++)
  {
    //dup new output target to stdout (closing stdout)
    if (!strcmp(args[i], ">") && args[i + 1] != NULL)
    {
      int nfd = open(args[i + 1], O_WRONLY | O_CREAT, 0644);
      dup2(nfd, 1);
    }
    else if (!strcmp(args[i], ">>") && args[i + 1] != NULL)
    {
      int nfd = open(args[i + 1], O_WRONLY | O_APPEND | O_CREAT, 0644);
      dup2(nfd, 1);
    }
    else if (!strcmp(args[i], "<") && args[i + 1] != NULL)
    {
      int nfd = open(args[i + 1], O_WRONLY | O_CREAT, 0644);
      dup2(nfd, 0);
    }
  }
}

int exec(char **args, char **env, char *cmd)
{
  //IO redirect
  IO_redirect(args);

  char temp[PATH_SIZE];
  for (int i = 0; i < ndir; i++)
  {
    strcpy(temp, dir[i]);
    strcat(temp, "/");
    strcat(temp, cmd);
    execve(temp, args, env);
  }
}

int pipe_cleaner(char **args, char **env, int *pd)
{
  for (int i = 0; args[i] != NULL; i++)
    fprintf(stderr, "args[%d] = %s\n", i, args[i]);

  if (pd)
  {
    close(pd[0]);
    dup2(pd[1], 1);
    close(pd[1]);
  }
  //there is a pipe
  if (!split_args(args))
  {
    fprintf(stderr, "cleaning pipes...\n");
    int lpd[2];
    pipe(lpd);
    int pid = fork();
    if (pid)
    {
      close(lpd[1]);
      dup2(lpd[0], 0);
      close(lpd[0]);
      exec(argT, env, argT[0]);
    }
    else
      pipe_cleaner(argH, env, lpd);
  }
  else
    {
      for (int i = 0; argH[i] != NULL; i++)
        fprintf(stderr, "argH[%d] = %s\n", i, argH[i]);
      exec(args, env, args[0]);
    }
}

int main(int argc, char *argv[], char *env[])
{
  int i;
  int pid, status;
  char *cmd;
  char line[PATH_SIZE];

  // The base code assume only ONE dir[0] -> "/bin"
  // YOU do the general case of many dirs from PATH !!!!
  read_path(env);

  // show dirs
  for (i = 0; i < ndir; i++)
    printf("dir[%d] = %s\n", i, dir[i]);

  while (1)
  {
    printf("sh %d running\n", getpid());
    printf("enter a command line : ");
    fgets(line, PATH_SIZE, stdin);
    line[strlen(line) - 1] = 0;
    if (line[0] == 0)
      continue;
    if (line[0] == ' ')
      continue;

    tokenize(line);

    for (i = 0; i < n; i++)
    { // show token strings
      printf("arg[%d] = %s\n", i, arg[i]);
    }
    // getchar();

    cmd = arg[0]; // line = arg0 arg1 arg2 ...

    if (strcmp(cmd, "cd") == 0)
    {
      chdir(arg[1]);
      continue;
    }
    if (strcmp(cmd, "exit") == 0)
      exit(0);

    pid = fork();

    if (pid)
    {
      printf("sh %d forked a child sh %d\n", getpid(), pid);
      printf("sh %d wait for child sh %d to terminate\n", getpid(), pid);
      pid = wait(&status);
      printf("ZOMBIE child=%d exitStatus=%x\n", pid, status);
      printf("main sh %d repeat loop\n", getpid());
    }
    else
    {
      printf("child sh %d running\n", getpid());

      pipe_cleaner(arg, env, NULL);

      exit(1);
    }
  }
}

/********************* YOU DO ***********************
1. I/O redirections:

Example: line = arg0 arg1 ... > argn-1

  check each arg[i]:
  if arg[i] = ">" {
     arg[i] = 0; // null terminated arg[ ] array 
     // do output redirection to arg[i+1] as in Page 131 of BOOK
  }
  Then execve() to change image


2. Pipes:

Single pipe   : cmd1 | cmd2 :  Chapter 3.10.3, 3.11.2

Multiple pipes: Chapter 3.11.2
****************************************************/
