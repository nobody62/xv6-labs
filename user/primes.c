#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winfinite-recursion"

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define RD 0
#define WR 1

const uint INT_LEN = sizeof(int);


int
get_first_data(int lpipe[2], int* dst)
{
  if(read(lpipe[RD], dst, INT_LEN) == INT_LEN){
    printf("prime %d\n", *dst);
    return 0;
  }
  return -1;
}

void 
transmit_data(int lpipe[2], int rpipe[2], int first)
{
  int data;
  while(read(lpipe[RD], &data, INT_LEN) == INT_LEN){
    if(data % first != 0){
      write(rpipe[WR], &data, INT_LEN);
    }
  }
  close(lpipe[RD]);
  close(rpipe[WR]);
}

void
primes(int lpipe[2])
{
  close(lpipe[WR]);
  int first;
  if(get_first_data(lpipe, &first) == 0){
    int p[2];
    pipe(p);
    transmit_data(lpipe, p, first);

    if(fork() == 0){
      primes(p);
    } else{
      close(p[RD]);
      close(p[WR]);
      close(lpipe[RD]);
      wait(0);
    }
  }
  // else{
  //   // exit(0);
  // }
  exit(0);
}

int
main(int argc, char *argv[])
{
  int p[2];
  pipe(p);

  for(int i=2;i<=35;++i){
    write(p[WR], &i, INT_LEN);
  }

  if(fork() == 0){
    primes(p);
  } else{
    close(p[RD]);
    close(p[WR]);
    wait(0);
  }

  exit(0);
}