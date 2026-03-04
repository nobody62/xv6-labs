#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{

  int p1[2];
  int p2[2];
  char msg;
  pipe(p1);
  pipe(p2);
  msg = 1;

  if(fork() == 0){  // child
    close(p1[1]);
    close(p2[0]);
    
    read(p1[0], &msg, 1);
    printf("%d: received ping\n", getpid());
    write(p2[1], &msg, 1);

    close(p1[0]);
    close(p2[1]);

    exit(0);
  } else{           // parent
    close(p1[0]);
    close(p2[1]);

    write(p1[1], &msg, 1);
    read(p2[0], &msg, 1);
    printf("%d: received pong\n", getpid());
    close(p1[1]);
    close(p2[0]);
  }

  exit(0);
}