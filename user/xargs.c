#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXARGC 32

int
main(int argc, char *argv[])
{
  char buf[512];
  char* p = buf;

  if(argc < 2 || argc > MAXARGC - 1){
    fprintf(2, "Usage: xargs command\n");
    exit(1); 
  }

  char* xargv[MAXARGC];
  int xargc = 0;
  for(int i=1;i<argc;++i){
    xargv[xargc++] = argv[i];
  }

  int n = 0;
  while((n = read(0, p, 1)) > 0 || p > buf){ // process lines
    if(*p == '\n' || n == 0){
      *p = 0;
      if(fork() == 0){
        xargv[xargc++] = buf;
        xargv[xargc] = 0;
        exec(xargv[0], xargv);
        exit(0);
      } else{
        wait(0);
        p = buf;
      }
    } else{
      p++;
      if(p - buf >= 512){
        exit(1);
      }
    }
  }

  exit(0);
}